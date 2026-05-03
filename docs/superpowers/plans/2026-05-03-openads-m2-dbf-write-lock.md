# OpenADS — M2 DBF Write + LockMgr Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Append, update, and logically delete records in CDX- and NTX-typed DBF tables, with byte-range locks coordinated through a `LockMgr` running in **Compatible** mode (Clipper/FoxPro byte ranges). End-to-end test: an L1 ABI sequence opens an empty table, appends rows, updates fields, marks one deleted, locks/unlocks records, and the resulting DBF reads back correctly through M1's read-only path.

**Architecture:** Extend the L4 driver trait with write entry points (`write_record_raw`, `append_record_raw`, `set_record_count`, `sync`); both `CdxDriver` and the new `NtxDriver` route through `dbf_common`. `Table` grows `append_record`, `set_field`, `mark_deleted`, `recall_deleted`, and `flush`. A new L4 `LockMgr` sits over `platform::ByteLock` and selects the byte-range scheme from the table type and `LockingMode`. `Connection::open_table` learns an `OpenMode` (Read / Shared / Exclusive) and a `LockingMode` (Compatible default). The L1 ABI gains 14 new entry points covering write and lock surfaces.

**Tech Stack:** Same as M0/M1. No new third-party deps.

---

## File structure for this milestone

Touched in M2:

```
OpenADS/
├── src/
│   ├── drivers/
│   │   ├── dbf_common.{h,cpp}      # encode_field / make_empty_record / write helpers
│   │   ├── driver_trait.h          # extended with write_record_raw / append / sync
│   │   ├── cdx/cdx_driver.{h,cpp}  # write paths + open mode (rw)
│   │   └── ntx/                    # NEW
│   │       ├── ntx_driver.h
│   │       └── ntx_driver.cpp
│   ├── engine/
│   │   ├── lock_mgr.h              # NEW
│   │   ├── lock_mgr.cpp            # NEW
│   │   ├── table.{h,cpp}           # write surface, open with mode + locking
│   ├── session/
│   │   └── connection.{h,cpp}      # open_table accepts mode + locking
│   └── abi/
│       └── ace_exports.cpp         # 14 new thunks
├── include/openads/ace.h           # 14 new declarations + ADS_*_LOCKING enums
└── tests/unit/
    ├── dbf_write_test.cpp          # NEW
    ├── ntx_driver_test.cpp         # NEW
    ├── lock_mgr_test.cpp           # NEW
    ├── engine_table_write_test.cpp # NEW
    └── abi_write_smoke_test.cpp    # NEW
```

Boundaries:

- `LockMgr` lives in `src/engine/` (not `platform/`): it knows about table types and the proprietary-vs-compatible split, while `platform::ByteLock` stays oblivious.
- `dbf_common` keeps all DBF-format knowledge — drivers stay thin wrappers.
- `NtxDriver` for now is byte-for-byte equivalent to `CdxDriver`; the only difference is the `TableType` label. The actual `.ntx` and `.cdx` index files come in M3, so this milestone's NTX driver is intentionally label-only.

---

## Task 1: Extend `IDriver` and `dbf_common` for writes

**Files:**
- Modify: `c:/OpenADS/src/drivers/driver_trait.h`
- Modify: `c:/OpenADS/src/drivers/dbf_common.h`
- Modify: `c:/OpenADS/src/drivers/dbf_common.cpp`
- Create: `c:/OpenADS/tests/unit/dbf_write_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/dbf_write_test.cpp` (or create it):

```cpp
#include "doctest.h"
#include "drivers/dbf_common.h"

#include <cstdint>
#include <string>
#include <vector>

using openads::drivers::DbfField;
using openads::drivers::DbfFieldType;
using openads::drivers::encode_field_string;
using openads::drivers::encode_field_double;
using openads::drivers::encode_field_logical;
using openads::drivers::make_empty_record;
using openads::drivers::set_record_deleted;

TEST_CASE("make_empty_record fills a buffer with the deletion byte and spaces") {
    std::vector<DbfField> fields;
    DbfField a; a.type = DbfFieldType::Character; a.length = 4; a.record_offset = 1;
    DbfField b; b.type = DbfFieldType::Numeric;   b.length = 3; b.record_offset = 5;
    fields.push_back(a);
    fields.push_back(b);

    std::uint16_t rec_len = 1 + 4 + 3;
    auto rec = make_empty_record(rec_len);
    REQUIRE(rec.size() == rec_len);
    CHECK(rec[0] == ' ');
    for (std::size_t i = 1; i < rec.size(); ++i) CHECK(rec[i] == ' ');
}

TEST_CASE("encode_field_string left-justifies and pads with spaces") {
    DbfField f;
    f.type = DbfFieldType::Character;
    f.length = 5;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 5);
    auto r = encode_field_string(f, rec.data(), rec.size(), "hi");
    REQUIRE(r.has_value());
    CHECK(rec[1] == 'h');
    CHECK(rec[2] == 'i');
    CHECK(rec[3] == ' ');
    CHECK(rec[4] == ' ');
    CHECK(rec[5] == ' ');
}

TEST_CASE("encode_field_string truncates oversized input to field length") {
    DbfField f;
    f.type = DbfFieldType::Character;
    f.length = 3;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 3);
    auto r = encode_field_string(f, rec.data(), rec.size(), "abcdef");
    REQUIRE(r.has_value());
    CHECK(rec[1] == 'a');
    CHECK(rec[2] == 'b');
    CHECK(rec[3] == 'c');
}

TEST_CASE("encode_field_double right-justifies a numeric field with decimals") {
    DbfField f;
    f.type = DbfFieldType::Numeric;
    f.length = 7;
    f.decimals = 2;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 7);
    auto r = encode_field_double(f, rec.data(), rec.size(), 3.14);
    REQUIRE(r.has_value());
    std::string s(reinterpret_cast<const char*>(rec.data() + 1), 7);
    CHECK(s == "   3.14");
}

TEST_CASE("encode_field_logical writes T or F") {
    DbfField f;
    f.type = DbfFieldType::Logical;
    f.length = 1;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 1);
    REQUIRE(encode_field_logical(f, rec.data(), rec.size(), true).has_value());
    CHECK(rec[1] == 'T');
    REQUIRE(encode_field_logical(f, rec.data(), rec.size(), false).has_value());
    CHECK(rec[1] == 'F');
}

TEST_CASE("set_record_deleted toggles the deletion byte") {
    auto rec = make_empty_record(4);
    set_record_deleted(rec.data(), rec.size(), true);
    CHECK(rec[0] == '*');
    set_record_deleted(rec.data(), rec.size(), false);
    CHECK(rec[0] == ' ');
}
```

- [ ] **Step 2: Extend `dbf_common.h`**

Append to `c:/OpenADS/src/drivers/dbf_common.h`, before the closing namespace brace:

```cpp
std::vector<std::uint8_t> make_empty_record(std::uint16_t record_length);

util::Result<void> encode_field_string (const DbfField& f,
                                        std::uint8_t* rec, std::size_t rec_size,
                                        const std::string& value);
util::Result<void> encode_field_double (const DbfField& f,
                                        std::uint8_t* rec, std::size_t rec_size,
                                        double value);
util::Result<void> encode_field_logical(const DbfField& f,
                                        std::uint8_t* rec, std::size_t rec_size,
                                        bool value);

void set_record_deleted(std::uint8_t* rec, std::size_t rec_size,
                        bool deleted) noexcept;
```

- [ ] **Step 3: Implement encoders in `dbf_common.cpp`**

Append to `c:/OpenADS/src/drivers/dbf_common.cpp`:

```cpp
namespace openads::drivers {

std::vector<std::uint8_t> make_empty_record(std::uint16_t record_length) {
    return std::vector<std::uint8_t>(record_length, ' ');
}

util::Result<void> encode_field_string(const DbfField& f,
                                       std::uint8_t* rec, std::size_t rec_size,
                                       const std::string& value) {
    if (static_cast<std::size_t>(f.record_offset) + f.length > rec_size) {
        return util::Error{5000, 0, "field range past record buffer", ""};
    }
    std::uint8_t* dst = rec + f.record_offset;
    std::size_t n = std::min<std::size_t>(value.size(), f.length);
    std::memcpy(dst, value.data(), n);
    for (std::size_t i = n; i < f.length; ++i) dst[i] = ' ';
    return {};
}

util::Result<void> encode_field_double(const DbfField& f,
                                       std::uint8_t* rec, std::size_t rec_size,
                                       double value) {
    if (static_cast<std::size_t>(f.record_offset) + f.length > rec_size) {
        return util::Error{5000, 0, "field range past record buffer", ""};
    }
    char tmp[64];
    int written = std::snprintf(tmp, sizeof(tmp), "%*.*f",
                                static_cast<int>(f.length),
                                static_cast<int>(f.decimals),
                                value);
    if (written < 0) {
        return util::Error{5000, 0, "snprintf failed encoding numeric", ""};
    }
    std::size_t n = static_cast<std::size_t>(written);
    if (n > f.length) n = f.length;       // truncate on overflow
    std::uint8_t* dst = rec + f.record_offset;
    std::memcpy(dst, tmp, n);
    for (std::size_t i = n; i < f.length; ++i) dst[i] = ' ';
    return {};
}

util::Result<void> encode_field_logical(const DbfField& f,
                                        std::uint8_t* rec, std::size_t rec_size,
                                        bool value) {
    if (static_cast<std::size_t>(f.record_offset) + f.length > rec_size) {
        return util::Error{5000, 0, "field range past record buffer", ""};
    }
    rec[f.record_offset] = value ? 'T' : 'F';
    return {};
}

void set_record_deleted(std::uint8_t* rec, std::size_t rec_size,
                        bool deleted) noexcept {
    if (rec_size == 0) return;
    rec[0] = deleted ? '*' : ' ';
}

} // namespace openads::drivers
```

- [ ] **Step 4: Extend `driver_trait.h` with write surface**

Replace `c:/OpenADS/src/drivers/driver_trait.h`:

```cpp
#pragma once

#include "drivers/dbf_common.h"
#include "platform/file.h"
#include "util/result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openads::drivers {

enum class DriverOpenMode {
    ReadOnly,
    Shared,    // read+write, multiple openers
    Exclusive  // read+write, sole opener
};

class IDriver {
public:
    virtual ~IDriver() = default;

    virtual util::Result<void>
        open(const std::string& path, DriverOpenMode mode) = 0;

    virtual std::uint32_t record_count() const noexcept = 0;
    virtual std::uint16_t record_length() const noexcept = 0;
    virtual std::uint16_t header_length() const noexcept = 0;
    virtual const std::vector<DbfField>& fields() const noexcept = 0;
    virtual platform::File& file() = 0;

    virtual util::Result<std::vector<std::uint8_t>>
        read_record_raw(std::uint32_t recno) = 0;

    virtual util::Result<void>
        write_record_raw(std::uint32_t recno,
                         const std::uint8_t* buf, std::size_t n) = 0;

    // Appends one record at the end. On success, the new recno equals
    // the post-call record_count().
    virtual util::Result<std::uint32_t>
        append_record_raw(const std::uint8_t* buf, std::size_t n) = 0;

    virtual util::Result<void> flush() = 0;
};

} // namespace openads::drivers
```

- [ ] **Step 5: Run tests to confirm encoder tests pass and read tests still pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 6 new test cases pass; the suite total grows by 6.

- [ ] **Step 6: Commit**

```
git add src/drivers/driver_trait.h src/drivers/dbf_common.h src/drivers/dbf_common.cpp tests/unit/dbf_write_test.cpp tests/CMakeLists.txt
git commit -m "feat(drivers): DBF field encoders and write surface in IDriver"
```

Note: `tests/CMakeLists.txt` is updated in the next step to register the new test file, but it must compile right away. Add `unit/dbf_write_test.cpp` to the executable list now.

---

## Task 2: Register new test files in CMake

**Files:**
- Modify: `c:/OpenADS/tests/CMakeLists.txt`

- [ ] **Step 1: Update test target**

Replace contents:

```cmake
add_executable(openads_unit_tests
    unit/doctest_main.cpp
    unit/util_result_test.cpp
    unit/util_span_test.cpp
    unit/util_log_test.cpp
    unit/platform_file_test.cpp
    unit/platform_lock_test.cpp
    unit/platform_mmap_test.cpp
    unit/platform_path_test.cpp
    unit/platform_time_test.cpp
    unit/platform_thread_test.cpp
    unit/dbf_header_test.cpp
    unit/dbf_field_test.cpp
    unit/dbf_record_test.cpp
    unit/dbf_write_test.cpp
    unit/ntx_driver_test.cpp
    unit/lock_mgr_test.cpp
    unit/engine_table_test.cpp
    unit/engine_table_write_test.cpp
    unit/engine_cursor_test.cpp
    unit/session_handle_registry_test.cpp
    unit/session_connection_test.cpp
    unit/abi_smoke_test.cpp
    unit/abi_write_smoke_test.cpp
)

target_include_directories(openads_unit_tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_include_directories(openads_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(openads_unit_tests PRIVATE openads_core)

add_test(NAME openads_unit_tests COMMAND openads_unit_tests)
```

- [ ] **Step 2: Stub new test files so the build passes**

Each new test file gets the placeholder:

```cpp
#include "doctest.h"
```

Files:

- `c:/OpenADS/tests/unit/ntx_driver_test.cpp`
- `c:/OpenADS/tests/unit/lock_mgr_test.cpp`
- `c:/OpenADS/tests/unit/engine_table_write_test.cpp`
- `c:/OpenADS/tests/unit/abi_write_smoke_test.cpp`

`unit/dbf_write_test.cpp` already has its real content from Task 1.

- [ ] **Step 3: Build to verify the wiring**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: success; suite still passes.

- [ ] **Step 4: Commit**

```
git add tests/CMakeLists.txt tests/unit/ntx_driver_test.cpp tests/unit/lock_mgr_test.cpp tests/unit/engine_table_write_test.cpp tests/unit/abi_write_smoke_test.cpp
git commit -m "build: register M2 test files (stubs to be filled in)"
```

---

## Task 3: Update `CdxDriver` for read+write open and write surface

**Files:**
- Modify: `c:/OpenADS/src/drivers/cdx/cdx_driver.h`
- Modify: `c:/OpenADS/src/drivers/cdx/cdx_driver.cpp`

- [ ] **Step 1: Replace `cdx_driver.h`**

```cpp
#pragma once

#include "drivers/driver_trait.h"
#include "platform/file.h"

namespace openads::drivers::cdx {

class CdxDriver final : public IDriver {
public:
    util::Result<void>
        open(const std::string& path, DriverOpenMode mode) override;

    std::uint32_t record_count() const noexcept override { return rec_count_; }
    std::uint16_t record_length() const noexcept override { return rec_len_; }
    std::uint16_t header_length() const noexcept override { return hdr_len_; }
    const std::vector<DbfField>& fields() const noexcept override { return fields_; }
    platform::File& file() override { return file_; }

    util::Result<std::vector<std::uint8_t>>
        read_record_raw(std::uint32_t recno) override;

    util::Result<void>
        write_record_raw(std::uint32_t recno,
                         const std::uint8_t* buf, std::size_t n) override;

    util::Result<std::uint32_t>
        append_record_raw(const std::uint8_t* buf, std::size_t n) override;

    util::Result<void> flush() override;

private:
    util::Result<void> rewrite_header_();

    platform::File          file_;
    std::vector<DbfField>   fields_;
    DriverOpenMode          mode_      = DriverOpenMode::ReadOnly;
    std::uint32_t           rec_count_ = 0;
    std::uint16_t           rec_len_   = 0;
    std::uint16_t           hdr_len_   = 0;
};

} // namespace openads::drivers::cdx
```

- [ ] **Step 2: Replace `cdx_driver.cpp`**

```cpp
#include "drivers/cdx/cdx_driver.h"

#include "platform/time.h"

#include <cstring>
#include <vector>

namespace openads::drivers::cdx {

namespace {

platform::OpenMode map_mode(DriverOpenMode m) {
    switch (m) {
        case DriverOpenMode::ReadOnly:  return platform::OpenMode::ReadOnly;
        case DriverOpenMode::Shared:    return platform::OpenMode::OpenExisting;
        case DriverOpenMode::Exclusive: return platform::OpenMode::OpenExisting;
    }
    return platform::OpenMode::ReadOnly;
}

} // namespace

util::Result<void>
CdxDriver::open(const std::string& path, DriverOpenMode mode) {
    mode_ = mode;
    auto fres = platform::File::open(path, map_mode(mode));
    if (!fres) return fres.error();
    file_ = std::move(fres).value();

    std::uint8_t hdr_buf[32]{};
    auto got = file_.read_at(0, hdr_buf, sizeof(hdr_buf));
    if (!got) return got.error();
    if (got.value() < 32) {
        return util::Error{5103, 0, "DBF header truncated", path};
    }

    auto hdr = parse_dbf_header(hdr_buf, sizeof(hdr_buf));
    if (!hdr) return hdr.error();
    rec_count_ = hdr.value().record_count;
    rec_len_   = hdr.value().record_length;
    hdr_len_   = hdr.value().header_length;

    if (hdr_len_ < 33) {
        return util::Error{5103, 0, "DBF header length below 33 bytes", path};
    }
    std::size_t fd_size = hdr_len_ - 32;
    std::vector<std::uint8_t> fd_buf(fd_size, 0);
    auto fd_got = file_.read_at(32, fd_buf.data(), fd_buf.size());
    if (!fd_got) return fd_got.error();
    if (fd_got.value() < fd_buf.size()) {
        return util::Error{5103, 0, "field-descriptor block truncated", path};
    }
    auto fields = parse_dbf_fields(fd_buf.data(), fd_buf.size());
    if (!fields) return fields.error();
    fields_ = std::move(fields).value();
    return {};
}

util::Result<std::vector<std::uint8_t>>
CdxDriver::read_record_raw(std::uint32_t recno) {
    if (recno == 0 || recno > rec_count_) {
        return util::Error{5000, 0, "record number out of range", ""};
    }
    std::vector<std::uint8_t> buf(rec_len_, 0);
    std::uint64_t offset = static_cast<std::uint64_t>(hdr_len_) +
                           static_cast<std::uint64_t>(recno - 1) *
                           static_cast<std::uint64_t>(rec_len_);
    auto got = file_.read_at(offset, buf.data(), buf.size());
    if (!got) return got.error();
    if (got.value() < buf.size()) {
        return util::Error{5000, 0, "short read on record body", ""};
    }
    return buf;
}

util::Result<void>
CdxDriver::write_record_raw(std::uint32_t recno,
                            const std::uint8_t* buf, std::size_t n) {
    if (mode_ == DriverOpenMode::ReadOnly) {
        return util::Error{5000, 0, "table opened read-only", ""};
    }
    if (recno == 0 || recno > rec_count_) {
        return util::Error{5000, 0, "record number out of range", ""};
    }
    if (n != rec_len_) {
        return util::Error{5000, 0, "record buffer length mismatch", ""};
    }
    std::uint64_t offset = static_cast<std::uint64_t>(hdr_len_) +
                           static_cast<std::uint64_t>(recno - 1) *
                           static_cast<std::uint64_t>(rec_len_);
    auto wrote = file_.write_at(offset, buf, n);
    if (!wrote) return wrote.error();
    if (wrote.value() != n) {
        return util::Error{5000, 0, "short write on record body", ""};
    }
    return {};
}

util::Result<std::uint32_t>
CdxDriver::append_record_raw(const std::uint8_t* buf, std::size_t n) {
    if (mode_ == DriverOpenMode::ReadOnly) {
        return util::Error{5000, 0, "table opened read-only", ""};
    }
    if (n != rec_len_) {
        return util::Error{5000, 0, "record buffer length mismatch", ""};
    }
    std::uint32_t new_recno = rec_count_ + 1;
    std::uint64_t offset = static_cast<std::uint64_t>(hdr_len_) +
                           static_cast<std::uint64_t>(rec_count_) *
                           static_cast<std::uint64_t>(rec_len_);
    auto wrote = file_.write_at(offset, buf, n);
    if (!wrote) return wrote.error();
    if (wrote.value() != n) {
        return util::Error{5000, 0, "short write on record body", ""};
    }
    // EOF marker (0x1A) directly after the new record.
    std::uint8_t eof = 0x1A;
    file_.write_at(offset + n, &eof, 1);

    rec_count_ = new_recno;
    if (auto r = rewrite_header_(); !r) return r.error();
    return new_recno;
}

util::Result<void> CdxDriver::rewrite_header_() {
    // Read, mutate fields 1-7 (last update + recno), write back.
    std::uint8_t hdr_buf[32]{};
    auto got = file_.read_at(0, hdr_buf, sizeof(hdr_buf));
    if (!got) return got.error();

    std::int64_t now = platform::utc_unix_micros();
    std::time_t  secs = static_cast<std::time_t>(now / 1'000'000);
    std::tm tm_utc{};
#ifdef _WIN32
    gmtime_s(&tm_utc, &secs);
#else
    gmtime_r(&secs, &tm_utc);
#endif
    hdr_buf[1] = static_cast<std::uint8_t>(tm_utc.tm_year);  // years since 1900
    hdr_buf[2] = static_cast<std::uint8_t>(tm_utc.tm_mon + 1);
    hdr_buf[3] = static_cast<std::uint8_t>(tm_utc.tm_mday);

    hdr_buf[4] = static_cast<std::uint8_t>( rec_count_        & 0xFFu);
    hdr_buf[5] = static_cast<std::uint8_t>((rec_count_ >> 8)  & 0xFFu);
    hdr_buf[6] = static_cast<std::uint8_t>((rec_count_ >> 16) & 0xFFu);
    hdr_buf[7] = static_cast<std::uint8_t>((rec_count_ >> 24) & 0xFFu);

    auto wrote = file_.write_at(0, hdr_buf, sizeof(hdr_buf));
    if (!wrote) return wrote.error();
    if (wrote.value() != sizeof(hdr_buf)) {
        return util::Error{5000, 0, "short write on header", ""};
    }
    return {};
}

util::Result<void> CdxDriver::flush() {
    return file_.sync();
}

} // namespace openads::drivers::cdx
```

- [ ] **Step 3: Build (smoke level — driver still without dedicated tests)**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: success; existing 53 cases still pass.

- [ ] **Step 4: Commit**

```
git add src/drivers/cdx/cdx_driver.h src/drivers/cdx/cdx_driver.cpp
git commit -m "feat(drivers): CdxDriver write+append, header rewrite on append"
```

---

## Task 4: NTX driver

**Files:**
- Create: `c:/OpenADS/src/drivers/ntx/ntx_driver.h`
- Create: `c:/OpenADS/src/drivers/ntx/ntx_driver.cpp`
- Modify: `c:/OpenADS/src/CMakeLists.txt`
- Modify: `c:/OpenADS/tests/unit/ntx_driver_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/ntx_driver_test.cpp`:

```cpp
#include "doctest.h"
#include "drivers/ntx/ntx_driver.h"
#include "drivers/dbf_common.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using openads::drivers::DriverOpenMode;
using openads::drivers::ntx::NtxDriver;

namespace {

fs::path make_dbf(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("openads_m2_ntx_") + tag);
    fs::remove(p);
    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03;
    hdr[4] = 1;
    hdr[8] = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 3; hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "T", 11);
    fd[11] = 'C';
    fd[16] = 3;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(' '); file.push_back('a'); file.push_back('b'); file.push_back('c');
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("NtxDriver opens a DBF and reports its layout") {
    auto p = make_dbf("layout");
    {
        NtxDriver d;
        auto r = d.open(p.string(), DriverOpenMode::Shared);
        REQUIRE(r.has_value());
        CHECK(d.record_count() == 1);
        CHECK(d.record_length() == 4);
        CHECK(d.fields().size() == 1);
        CHECK(d.fields()[0].name == "T");
    }
    fs::remove(p);
}
```

- [ ] **Step 2: Implement `ntx_driver.h`**

Write `c:/OpenADS/src/drivers/ntx/ntx_driver.h`:

```cpp
#pragma once

#include "drivers/driver_trait.h"
#include "drivers/cdx/cdx_driver.h"

namespace openads::drivers::ntx {

// In M2 the NTX driver is a label-only specialisation: the .dbf bytes
// look identical to a CDX-typed file. The .ntx index file lands in M3.
class NtxDriver final : public IDriver {
public:
    util::Result<void>
        open(const std::string& path, DriverOpenMode mode) override
    { return inner_.open(path, mode); }

    std::uint32_t record_count() const noexcept override
    { return inner_.record_count(); }
    std::uint16_t record_length() const noexcept override
    { return inner_.record_length(); }
    std::uint16_t header_length() const noexcept override
    { return inner_.header_length(); }
    const std::vector<DbfField>& fields() const noexcept override
    { return inner_.fields(); }
    platform::File& file() override { return inner_.file(); }

    util::Result<std::vector<std::uint8_t>>
        read_record_raw(std::uint32_t recno) override
    { return inner_.read_record_raw(recno); }

    util::Result<void>
        write_record_raw(std::uint32_t recno,
                         const std::uint8_t* buf, std::size_t n) override
    { return inner_.write_record_raw(recno, buf, n); }

    util::Result<std::uint32_t>
        append_record_raw(const std::uint8_t* buf, std::size_t n) override
    { return inner_.append_record_raw(buf, n); }

    util::Result<void> flush() override { return inner_.flush(); }

private:
    cdx::CdxDriver inner_;
};

} // namespace openads::drivers::ntx
```

- [ ] **Step 3: Implement `ntx_driver.cpp`**

Write `c:/OpenADS/src/drivers/ntx/ntx_driver.cpp`:

```cpp
#include "drivers/ntx/ntx_driver.h"

// All members are header-defined for now.
namespace openads::drivers::ntx {} // namespace
```

- [ ] **Step 4: Wire into `src/CMakeLists.txt`**

Add `drivers/ntx/ntx_driver.cpp` to the source list under `add_library(openads_core STATIC ...)`.

The replaced file is:

```cmake
add_library(openads_core STATIC
    util/log.cpp
    platform/path.cpp
    platform/time.cpp
    platform/thread.cpp
    abi/ace_exports.cpp
    abi/last_error.cpp
    abi/charset.cpp
    session/handle_registry.cpp
    session/connection.cpp
    engine/table.cpp
    engine/cursor.cpp
    engine/lock_mgr.cpp
    drivers/dbf_common.cpp
    drivers/cdx/cdx_driver.cpp
    drivers/ntx/ntx_driver.cpp
)

if(WIN32)
    target_sources(openads_core PRIVATE
        platform/file_win32.cpp
        platform/lock_win32.cpp
        platform/mmap_win32.cpp
    )
else()
    target_sources(openads_core PRIVATE
        platform/file_posix.cpp
        platform/lock_posix.cpp
        platform/mmap_posix.cpp
    )
endif()

target_include_directories(openads_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(UNIX)
    target_link_libraries(openads_core PUBLIC pthread)
endif()
```

(The `engine/lock_mgr.cpp` source is added now; the file is created in Task 5. To keep this task self-contained, also create a placeholder `c:/OpenADS/src/engine/lock_mgr.cpp` containing the single line `// placeholder, real content lands in a later task` and a placeholder header `c:/OpenADS/src/engine/lock_mgr.h` containing only `#pragma once`.)

- [ ] **Step 5: Build + test**

Run:

```
cmake --preset default
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: NTX test passes; suite still green.

- [ ] **Step 6: Commit**

```
git add src/drivers/ntx/ntx_driver.h src/drivers/ntx/ntx_driver.cpp src/CMakeLists.txt src/engine/lock_mgr.h src/engine/lock_mgr.cpp tests/unit/ntx_driver_test.cpp
git commit -m "feat(drivers): NtxDriver wrapping CdxDriver (label-only until M3)"
```

---

## Task 5: `LockMgr` with Compatible-mode byte ranges

**Files:**
- Modify: `c:/OpenADS/src/engine/lock_mgr.h`
- Modify: `c:/OpenADS/src/engine/lock_mgr.cpp`
- Modify: `c:/OpenADS/tests/unit/lock_mgr_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/lock_mgr_test.cpp`:

```cpp
#include "doctest.h"
#include "engine/lock_mgr.h"
#include "platform/file.h"

#include <filesystem>

namespace fs = std::filesystem;
using openads::engine::LockMgr;
using openads::engine::LockingMode;
using openads::engine::TableTypeForLock;
using openads::platform::File;
using openads::platform::OpenMode;

TEST_CASE("LockMgr Compatible/NTX file lock at byte 1_000_000_000") {
    auto p = fs::temp_directory_path() / "openads_m2_lock_ntx";
    fs::remove(p);
    {
        auto fres = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(fres.has_value());
        File f = std::move(fres).value();
        LockMgr mgr;
        auto lock = mgr.lock_table_excl(f, TableTypeForLock::Ntx,
                                        LockingMode::Compatible);
        REQUIRE(lock.has_value());
        // The lock token holds the actual range used; verify it.
        CHECK(lock.value().offset() == 1'000'000'000ull);
        CHECK(lock.value().length() == 1ull);
    }
    fs::remove(p);
}

TEST_CASE("LockMgr Compatible/CDX record lock at 0x7FFFFFFE - recno") {
    auto p = fs::temp_directory_path() / "openads_m2_lock_cdx";
    fs::remove(p);
    {
        auto fres = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(fres.has_value());
        File f = std::move(fres).value();
        LockMgr mgr;
        auto lock = mgr.lock_record_excl(f, TableTypeForLock::Cdx,
                                         LockingMode::Compatible, 7);
        REQUIRE(lock.has_value());
        CHECK(lock.value().offset() == (0x7FFFFFFEull - 7ull));
        CHECK(lock.value().length() == 1ull);
    }
    fs::remove(p);
}

TEST_CASE("LockMgr re-entrant record lock from same handle is a no-op") {
    auto p = fs::temp_directory_path() / "openads_m2_lock_reenter";
    fs::remove(p);
    {
        auto fres = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(fres.has_value());
        File f = std::move(fres).value();
        LockMgr mgr;
        auto a = mgr.lock_record_excl(f, TableTypeForLock::Cdx,
                                      LockingMode::Compatible, 42);
        REQUIRE(a.has_value());
        // Second acquire of the same range should succeed without OS recursion.
        auto b = mgr.lock_record_excl(f, TableTypeForLock::Cdx,
                                      LockingMode::Compatible, 42);
        REQUIRE(b.has_value());
    }
    fs::remove(p);
}
```

- [ ] **Step 2: Replace `engine/lock_mgr.h`**

```cpp
#pragma once

#include "platform/file.h"
#include "platform/lock.h"
#include "util/result.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace openads::engine {

enum class TableTypeForLock { Ntx, Cdx, Vfp, Adt };
enum class LockingMode      { Compatible, Proprietary };

// Token holding the OS lock plus the chosen range, so callers can
// inspect what was acquired (used by tests, observability).
class LockHandle {
public:
    LockHandle() = default;
    LockHandle(platform::ByteLock&& lk,
               std::uint64_t offset, std::uint64_t length) noexcept
        : lock_(std::move(lk)), offset_(offset), length_(length) {}

    LockHandle(LockHandle&&) noexcept = default;
    LockHandle& operator=(LockHandle&&) noexcept = default;

    std::uint64_t offset() const noexcept { return offset_; }
    std::uint64_t length() const noexcept { return length_; }

    void release() noexcept { lock_.release(); }

private:
    platform::ByteLock lock_;
    std::uint64_t      offset_ = 0;
    std::uint64_t      length_ = 0;
};

class LockMgr {
public:
    util::Result<LockHandle>
        lock_table_excl (platform::File& f,
                         TableTypeForLock t, LockingMode m);

    util::Result<LockHandle>
        lock_record_excl(platform::File& f,
                         TableTypeForLock t, LockingMode m,
                         std::uint32_t recno);

    util::Result<LockHandle>
        lock_record_shared(platform::File& f,
                           TableTypeForLock t, LockingMode m,
                           std::uint32_t recno);

    // Public helpers so tests and the engine can compute the same
    // ranges without acquiring a real lock.
    static std::uint64_t file_lock_offset(TableTypeForLock t, LockingMode m);
    static std::uint64_t record_lock_offset(TableTypeForLock t, LockingMode m,
                                            std::uint32_t recno);

private:
    // Per-(file*, range) re-entrancy counter; second acquire for the
    // same key returns success without calling the OS twice.
    struct Key {
        const void*   file;
        std::uint64_t offset;
        bool operator==(const Key& o) const noexcept {
            return file == o.file && offset == o.offset;
        }
    };
    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            return std::hash<const void*>{}(k.file) ^
                   std::hash<std::uint64_t>{}(k.offset);
        }
    };
    std::unordered_map<Key, int, KeyHash> held_;
};

} // namespace openads::engine
```

- [ ] **Step 3: Replace `engine/lock_mgr.cpp`**

```cpp
#include "engine/lock_mgr.h"

namespace openads::engine {

namespace {

constexpr std::uint64_t NTX_FILE_BASE = 1'000'000'000ULL;
constexpr std::uint64_t NTX_REC_BASE  = 1'000'000'001ULL;
constexpr std::uint64_t CDX_FILE_BASE = 0x7FFFFFFEULL;
constexpr std::uint64_t VFP_FILE_BASE = 0x3FFFFFFEULL;
constexpr std::uint64_t ADT_FILE_BASE = 0x80000000'00000000ULL;
constexpr std::uint64_t ADT_FILE_LEN  = 0x10000ULL;

} // namespace

std::uint64_t LockMgr::file_lock_offset(TableTypeForLock t, LockingMode m) {
    switch (t) {
        case TableTypeForLock::Ntx: return NTX_FILE_BASE;
        case TableTypeForLock::Cdx: return CDX_FILE_BASE;
        case TableTypeForLock::Vfp: return VFP_FILE_BASE;
        case TableTypeForLock::Adt: return ADT_FILE_BASE;
    }
    (void)m;
    return NTX_FILE_BASE;
}

std::uint64_t LockMgr::record_lock_offset(TableTypeForLock t, LockingMode m,
                                          std::uint32_t recno) {
    (void)m;
    switch (t) {
        case TableTypeForLock::Ntx: return NTX_REC_BASE  + recno;
        case TableTypeForLock::Cdx: return CDX_FILE_BASE - recno;
        case TableTypeForLock::Vfp: return VFP_FILE_BASE - recno;
        case TableTypeForLock::Adt: return ADT_FILE_BASE +
                                           (static_cast<std::uint64_t>(recno) << 16);
    }
    return NTX_REC_BASE + recno;
}

util::Result<LockHandle>
LockMgr::lock_table_excl(platform::File& f, TableTypeForLock t, LockingMode m) {
    std::uint64_t off = file_lock_offset(t, m);
    std::uint64_t len = (t == TableTypeForLock::Adt) ? ADT_FILE_LEN : 1ULL;
    Key k{&f, off};
    auto it = held_.find(k);
    if (it != held_.end()) {
        ++it->second;
        return LockHandle{platform::ByteLock{}, off, len};
    }
    auto bl = platform::ByteLock::acquire(f, off, len, platform::LockKind::Exclusive);
    if (!bl) return bl.error();
    held_[k] = 1;
    return LockHandle{std::move(bl).value(), off, len};
}

util::Result<LockHandle>
LockMgr::lock_record_excl(platform::File& f, TableTypeForLock t, LockingMode m,
                          std::uint32_t recno) {
    std::uint64_t off = record_lock_offset(t, m, recno);
    Key k{&f, off};
    auto it = held_.find(k);
    if (it != held_.end()) {
        ++it->second;
        return LockHandle{platform::ByteLock{}, off, 1};
    }
    auto bl = platform::ByteLock::acquire(f, off, 1, platform::LockKind::Exclusive);
    if (!bl) return bl.error();
    held_[k] = 1;
    return LockHandle{std::move(bl).value(), off, 1};
}

util::Result<LockHandle>
LockMgr::lock_record_shared(platform::File& f, TableTypeForLock t, LockingMode m,
                            std::uint32_t recno) {
    std::uint64_t off = record_lock_offset(t, m, recno);
    Key k{&f, off};
    auto it = held_.find(k);
    if (it != held_.end()) {
        ++it->second;
        return LockHandle{platform::ByteLock{}, off, 1};
    }
    auto bl = platform::ByteLock::acquire(f, off, 1, platform::LockKind::Shared);
    if (!bl) return bl.error();
    held_[k] = 1;
    return LockHandle{std::move(bl).value(), off, 1};
}

} // namespace openads::engine
```

- [ ] **Step 4: Build + test**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 3 new test cases pass.

- [ ] **Step 5: Commit**

```
git add src/engine/lock_mgr.h src/engine/lock_mgr.cpp tests/unit/lock_mgr_test.cpp
git commit -m "feat(engine): LockMgr with Compatible-mode byte ranges and re-entrancy"
```

---

## Task 6: Engine Table write surface

**Files:**
- Modify: `c:/OpenADS/src/engine/table.h`
- Modify: `c:/OpenADS/src/engine/table.cpp`
- Modify: `c:/OpenADS/tests/unit/engine_table_write_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/engine_table_write_test.cpp`:

```cpp
#include "doctest.h"
#include "engine/table.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using openads::engine::OpenMode;
using openads::engine::Table;
using openads::engine::TableType;

namespace {

fs::path make_empty_table(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("openads_m2_w_") + tag);
    fs::remove(p);

    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 0; // 0 records
    hdr[8]  = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 5; hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());

    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "NAME", 11);
    fd[11] = 'C';
    fd[16] = 5;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("Table append + set_field grows the file and round-trips") {
    auto p = make_empty_table("append");
    {
        auto t = Table::open(p.string(), TableType::Cdx, OpenMode::Shared);
        REQUIRE(t.has_value());
        Table table = std::move(t).value();

        REQUIRE(table.append_record().has_value());
        CHECK(table.recno() == 1);
        REQUIRE(table.set_field(0, std::string("Anna")).has_value());
        REQUIRE(table.flush().has_value());

        REQUIRE(table.append_record().has_value());
        CHECK(table.recno() == 2);
        REQUIRE(table.set_field(0, std::string("Bob")).has_value());
        REQUIRE(table.flush().has_value());
    }
    // Reopen read-only and verify both rows landed.
    {
        auto t = Table::open(p.string(), TableType::Cdx, OpenMode::Read);
        REQUIRE(t.has_value());
        Table table = std::move(t).value();
        CHECK(table.record_count() == 2);

        REQUIRE(table.goto_top().has_value());
        auto v0 = table.read_field(0);
        REQUIRE(v0.has_value());
        CHECK(v0.value().as_string == "Anna");

        REQUIRE(table.skip(1).has_value());
        auto v1 = table.read_field(0);
        REQUIRE(v1.has_value());
        CHECK(v1.value().as_string == "Bob");
    }
    fs::remove(p);
}

TEST_CASE("Table mark_deleted / recall_deleted toggle the deletion byte") {
    auto p = make_empty_table("delete");
    {
        auto t = Table::open(p.string(), TableType::Cdx, OpenMode::Shared);
        REQUIRE(t.has_value());
        Table table = std::move(t).value();
        REQUIRE(table.append_record().has_value());
        REQUIRE(table.set_field(0, std::string("X")).has_value());
        REQUIRE(table.mark_deleted().has_value());
        CHECK(table.is_deleted());
        REQUIRE(table.recall_deleted().has_value());
        CHECK_FALSE(table.is_deleted());
        REQUIRE(table.flush().has_value());
    }
    fs::remove(p);
}
```

- [ ] **Step 2: Replace `engine/table.h`**

```cpp
#pragma once

#include "drivers/driver_trait.h"
#include "drivers/dbf_common.h"
#include "engine/lock_mgr.h"
#include "util/result.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openads::engine {

enum class TableType { Cdx, Ntx, Adt, Vfp };
enum class OpenMode  { Read, Shared, Exclusive };

class Table {
public:
    Table() = default;
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;
    Table(Table&&) noexcept = default;
    Table& operator=(Table&&) noexcept = default;
    ~Table() = default;

    static util::Result<Table> open(const std::string& path,
                                    TableType type,
                                    OpenMode mode = OpenMode::Read,
                                    LockingMode locking = LockingMode::Compatible);

    std::uint16_t field_count() const noexcept;
    const drivers::DbfField& field_descriptor(std::uint16_t idx) const;
    std::int32_t field_index(const std::string& name) const noexcept;

    std::uint32_t record_count() const noexcept;
    std::uint32_t recno() const noexcept { return recno_; }
    bool eof() const noexcept { return state_ == State::Eof; }
    bool bof() const noexcept { return state_ == State::Bof; }

    util::Result<void> goto_top();
    util::Result<void> goto_bottom();
    util::Result<void> goto_record(std::uint32_t recno);
    util::Result<void> skip(std::int32_t delta);

    util::Result<drivers::DbfFieldValue>
        read_field(std::uint16_t field_index);

    // Write surface.
    util::Result<void> append_record();
    util::Result<void> set_field(std::uint16_t field_index,
                                 const std::string& value);
    util::Result<void> set_field(std::uint16_t field_index, double value);
    util::Result<void> set_field(std::uint16_t field_index, bool value);
    util::Result<void> mark_deleted();
    util::Result<void> recall_deleted();
    bool               is_deleted() const noexcept;
    util::Result<void> flush();

    // Locking surface.
    util::Result<void> lock_record_excl(std::uint32_t recno);
    util::Result<void> unlock_record    (std::uint32_t recno);
    util::Result<void> lock_table_excl();
    util::Result<void> unlock_table();

private:
    enum class State { Bof, Positioned, Eof };

    Table(std::unique_ptr<drivers::IDriver> drv,
          OpenMode mode, LockingMode locking, TableType type) noexcept
        : driver_(std::move(drv)), mode_(mode),
          locking_(locking), type_(type) {}

    util::Result<void> load_record_(std::uint32_t recno);
    util::Result<void> writeback_record_();

    TableTypeForLock to_lock_type_() const noexcept;

    std::unique_ptr<drivers::IDriver>            driver_;
    OpenMode                                     mode_     = OpenMode::Read;
    LockingMode                                  locking_  = LockingMode::Compatible;
    TableType                                    type_     = TableType::Cdx;
    LockMgr                                      locks_;
    std::unordered_map<std::uint32_t, LockHandle> recno_locks_;
    std::optional<LockHandle>                    table_lock_;
    State                                        state_  = State::Bof;
    std::uint32_t                                recno_  = 0;
    std::vector<std::uint8_t>                    record_buf_;
};

} // namespace openads::engine
```

The test uses `<unordered_map>` and `<optional>` indirectly; add these includes at the top of the header:

```cpp
#include <optional>
#include <unordered_map>
```

- [ ] **Step 3: Replace `engine/table.cpp`**

```cpp
#include "engine/table.h"

#include "drivers/cdx/cdx_driver.h"
#include "drivers/ntx/ntx_driver.h"

#include <utility>

namespace openads::engine {

util::Result<Table> Table::open(const std::string& path,
                                TableType type,
                                OpenMode mode,
                                LockingMode locking) {
    std::unique_ptr<drivers::IDriver> drv;
    switch (type) {
        case TableType::Cdx:
            drv = std::make_unique<drivers::cdx::CdxDriver>();
            break;
        case TableType::Ntx:
            drv = std::make_unique<drivers::ntx::NtxDriver>();
            break;
        case TableType::Adt:
        case TableType::Vfp:
            return util::Error{5004, 0,
                               "table type not yet supported in M2", path};
    }
    drivers::DriverOpenMode dmode = drivers::DriverOpenMode::ReadOnly;
    switch (mode) {
        case OpenMode::Read:      dmode = drivers::DriverOpenMode::ReadOnly;  break;
        case OpenMode::Shared:    dmode = drivers::DriverOpenMode::Shared;    break;
        case OpenMode::Exclusive: dmode = drivers::DriverOpenMode::Exclusive; break;
    }
    if (auto r = drv->open(path, dmode); !r) return r.error();
    return Table{std::move(drv), mode, locking, type};
}

std::uint16_t Table::field_count() const noexcept {
    return static_cast<std::uint16_t>(driver_->fields().size());
}

const drivers::DbfField& Table::field_descriptor(std::uint16_t idx) const {
    return driver_->fields().at(idx);
}

std::int32_t Table::field_index(const std::string& name) const noexcept {
    const auto& fs = driver_->fields();
    for (std::size_t i = 0; i < fs.size(); ++i) {
        if (fs[i].name == name) return static_cast<std::int32_t>(i);
    }
    return -1;
}

std::uint32_t Table::record_count() const noexcept {
    return driver_->record_count();
}

util::Result<void> Table::load_record_(std::uint32_t recno) {
    auto buf = driver_->read_record_raw(recno);
    if (!buf) return buf.error();
    record_buf_ = std::move(buf).value();
    recno_      = recno;
    state_      = State::Positioned;
    return {};
}

util::Result<void> Table::writeback_record_() {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    return driver_->write_record_raw(recno_, record_buf_.data(),
                                     record_buf_.size());
}

util::Result<void> Table::goto_top() {
    if (driver_->record_count() == 0) {
        state_ = State::Eof; recno_ = 0; return {};
    }
    return load_record_(1);
}

util::Result<void> Table::goto_bottom() {
    auto n = driver_->record_count();
    if (n == 0) { state_ = State::Eof; recno_ = 0; return {}; }
    return load_record_(n);
}

util::Result<void> Table::goto_record(std::uint32_t recno) {
    if (recno == 0 || recno > driver_->record_count()) {
        state_ = State::Eof; recno_ = 0;
        return util::Error{5000, 0, "recno out of range", ""};
    }
    return load_record_(recno);
}

util::Result<void> Table::skip(std::int32_t delta) {
    auto n = driver_->record_count();
    if (n == 0) { state_ = State::Eof; recno_ = 0; return {}; }
    std::int64_t target = static_cast<std::int64_t>(recno_) + delta;
    if (state_ == State::Bof && delta > 0) target = delta;
    if (target < 1) { state_ = State::Bof; recno_ = 0; return {}; }
    if (target > static_cast<std::int64_t>(n)) {
        state_ = State::Eof; recno_ = n + 1; return {};
    }
    return load_record_(static_cast<std::uint32_t>(target));
}

util::Result<drivers::DbfFieldValue>
Table::read_field(std::uint16_t field_index) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "table not positioned on a record", ""};
    }
    if (field_index >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    return drivers::decode_field(driver_->fields().at(field_index),
                                 record_buf_.data(), record_buf_.size());
}

util::Result<void> Table::append_record() {
    if (mode_ == OpenMode::Read) {
        return util::Error{5000, 0, "table opened read-only", ""};
    }
    auto rec = drivers::make_empty_record(driver_->record_length());
    auto new_recno = driver_->append_record_raw(rec.data(), rec.size());
    if (!new_recno) return new_recno.error();
    record_buf_ = std::move(rec);
    recno_      = new_recno.value();
    state_      = State::Positioned;
    return {};
}

util::Result<void> Table::set_field(std::uint16_t idx, const std::string& v) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    if (idx >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    auto r = drivers::encode_field_string(driver_->fields().at(idx),
                                          record_buf_.data(),
                                          record_buf_.size(), v);
    if (!r) return r.error();
    return writeback_record_();
}

util::Result<void> Table::set_field(std::uint16_t idx, double v) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    if (idx >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    auto r = drivers::encode_field_double(driver_->fields().at(idx),
                                          record_buf_.data(),
                                          record_buf_.size(), v);
    if (!r) return r.error();
    return writeback_record_();
}

util::Result<void> Table::set_field(std::uint16_t idx, bool v) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    if (idx >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    auto r = drivers::encode_field_logical(driver_->fields().at(idx),
                                           record_buf_.data(),
                                           record_buf_.size(), v);
    if (!r) return r.error();
    return writeback_record_();
}

util::Result<void> Table::mark_deleted() {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    drivers::set_record_deleted(record_buf_.data(), record_buf_.size(), true);
    return writeback_record_();
}

util::Result<void> Table::recall_deleted() {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    drivers::set_record_deleted(record_buf_.data(), record_buf_.size(), false);
    return writeback_record_();
}

bool Table::is_deleted() const noexcept {
    if (state_ != State::Positioned) return false;
    return drivers::record_is_deleted(record_buf_.data(), record_buf_.size());
}

util::Result<void> Table::flush() {
    return driver_->flush();
}

TableTypeForLock Table::to_lock_type_() const noexcept {
    switch (type_) {
        case TableType::Cdx: return TableTypeForLock::Cdx;
        case TableType::Ntx: return TableTypeForLock::Ntx;
        case TableType::Adt: return TableTypeForLock::Adt;
        case TableType::Vfp: return TableTypeForLock::Vfp;
    }
    return TableTypeForLock::Cdx;
}

util::Result<void> Table::lock_record_excl(std::uint32_t recno) {
    if (mode_ == OpenMode::Read) return {};
    auto h = locks_.lock_record_excl(driver_->file(), to_lock_type_(),
                                     locking_, recno);
    if (!h) return h.error();
    recno_locks_.emplace(recno, std::move(h).value());
    return {};
}

util::Result<void> Table::unlock_record(std::uint32_t recno) {
    auto it = recno_locks_.find(recno);
    if (it != recno_locks_.end()) {
        it->second.release();
        recno_locks_.erase(it);
    }
    return {};
}

util::Result<void> Table::lock_table_excl() {
    if (mode_ == OpenMode::Read) return {};
    auto h = locks_.lock_table_excl(driver_->file(), to_lock_type_(), locking_);
    if (!h) return h.error();
    table_lock_ = std::move(h).value();
    return {};
}

util::Result<void> Table::unlock_table() {
    if (table_lock_) {
        table_lock_->release();
        table_lock_.reset();
    }
    return {};
}

} // namespace openads::engine
```

- [ ] **Step 4: Update `Connection` to forward the open mode + locking**

Replace the `open_table` declaration in `c:/OpenADS/src/session/connection.h`:

```cpp
util::Result<Handle>
    open_table(const std::string& relative_path,
               engine::TableType  type,
               engine::OpenMode   mode = engine::OpenMode::Shared,
               engine::LockingMode locking = engine::LockingMode::Compatible);
```

Update the implementation in `c:/OpenADS/src/session/connection.cpp` body of `open_table`:

```cpp
util::Result<Handle> Connection::open_table(const std::string& relative_path,
                                            engine::TableType  type,
                                            engine::OpenMode   mode,
                                            engine::LockingMode locking) {
    namespace fs = std::filesystem;
    fs::path full = fs::path(data_dir_) / relative_path;
    auto resolved = platform::resolve_case_insensitive(full.string());

    auto t = engine::Table::open(resolved, type, mode, locking);
    if (!t) return t.error();

    auto holder = std::make_unique<engine::Table>(std::move(t).value());
    Handle h = next_table_handle_++;
    tables_.emplace(h, std::move(holder));
    return h;
}
```

- [ ] **Step 5: Build + test**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 2 new test cases pass.

- [ ] **Step 6: Commit**

```
git add src/engine/table.h src/engine/table.cpp src/session/connection.h src/session/connection.cpp tests/unit/engine_table_write_test.cpp
git commit -m "feat(engine): Table write surface (append / set_field / delete) plus locking"
```

---

## Task 7: Update L1 ABI with the M2 entry points

**Files:**
- Modify: `c:/OpenADS/include/openads/ace.h`
- Modify: `c:/OpenADS/src/abi/ace_exports.cpp`

- [ ] **Step 1: Append to `include/openads/ace.h`**

Inside the `extern "C"` block, before `#define ADS_FIELD_TYPE_CHAR`:

```cpp
UNSIGNED32 AdsAppendRecord  (ADSHANDLE hTable);
UNSIGNED32 AdsWriteRecord   (ADSHANDLE hTable);
UNSIGNED32 AdsDeleteRecord  (ADSHANDLE hTable);
UNSIGNED32 AdsRecallRecord  (ADSHANDLE hTable);
UNSIGNED32 AdsIsRecordDeleted(ADSHANDLE hTable, UNSIGNED16* pbDeleted);

UNSIGNED32 AdsSetString     (ADSHANDLE hTable, UNSIGNED8* pucField,
                              UNSIGNED8* pucValue, UNSIGNED32 ulLen);
UNSIGNED32 AdsSetLogical    (ADSHANDLE hTable, UNSIGNED8* pucField,
                              UNSIGNED16 bValue);
UNSIGNED32 AdsSetDouble     (ADSHANDLE hTable, UNSIGNED8* pucField,
                              double dValue);

UNSIGNED32 AdsLockRecord    (ADSHANDLE hTable, UNSIGNED32 ulRecord);
UNSIGNED32 AdsUnlockRecord  (ADSHANDLE hTable, UNSIGNED32 ulRecord);
UNSIGNED32 AdsLockTable     (ADSHANDLE hTable);
UNSIGNED32 AdsUnlockTable   (ADSHANDLE hTable);

UNSIGNED32 AdsFlushFileBuffers(ADSHANDLE hTable);
```

- [ ] **Step 2: Append the implementations to `src/abi/ace_exports.cpp`**

Inside the existing `extern "C"` block (before its closing brace):

```cpp
UNSIGNED32 AdsAppendRecord(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->append_record();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsWriteRecord(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->flush();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsDeleteRecord(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->mark_deleted();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsRecallRecord(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->recall_deleted();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsIsRecordDeleted(ADSHANDLE hTable, UNSIGNED16* pbDeleted) {
    Table* t = get_table(hTable);
    if (!t || pbDeleted == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    *pbDeleted = t->is_deleted() ? 1 : 0;
    return ok();
}

UNSIGNED32 AdsSetString(ADSHANDLE hTable, UNSIGNED8* pucField,
                        UNSIGNED8* pucValue, UNSIGNED32 ulLen) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto name = openads::abi::to_internal(pucField, 0);
    auto idx  = t->field_index(name);
    if (idx < 0) return fail(openads::AE_COLUMN_NOT_FOUND, name.c_str());
    std::string val(reinterpret_cast<const char*>(pucValue), ulLen);
    auto r = t->set_field(static_cast<std::uint16_t>(idx), val);
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsSetLogical(ADSHANDLE hTable, UNSIGNED8* pucField,
                         UNSIGNED16 bValue) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto name = openads::abi::to_internal(pucField, 0);
    auto idx  = t->field_index(name);
    if (idx < 0) return fail(openads::AE_COLUMN_NOT_FOUND, name.c_str());
    auto r = t->set_field(static_cast<std::uint16_t>(idx), bValue != 0);
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsSetDouble(ADSHANDLE hTable, UNSIGNED8* pucField,
                        double dValue) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto name = openads::abi::to_internal(pucField, 0);
    auto idx  = t->field_index(name);
    if (idx < 0) return fail(openads::AE_COLUMN_NOT_FOUND, name.c_str());
    auto r = t->set_field(static_cast<std::uint16_t>(idx), dValue);
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsLockRecord(ADSHANDLE hTable, UNSIGNED32 ulRecord) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->lock_record_excl(ulRecord);
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsUnlockRecord(ADSHANDLE hTable, UNSIGNED32 ulRecord) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->unlock_record(ulRecord);
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsLockTable(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->lock_table_excl();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsUnlockTable(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->unlock_table();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsFlushFileBuffers(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->flush();
    if (!r) return fail(r.error());
    return ok();
}
```

Also update `AdsOpenTable` to pass `engine::OpenMode::Shared` + `LockingMode::Compatible` to the new `Connection::open_table` signature; the simplest change is just to leave the call unchanged (defaults already pick `Shared` + `Compatible`).

- [ ] **Step 3: Build + run all tests**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: success; existing assertions still green.

- [ ] **Step 4: Commit**

```
git add include/openads/ace.h src/abi/ace_exports.cpp
git commit -m "feat(abi): M2 write and lock entry points"
```

---

## Task 8: ABI write smoke test

**Files:**
- Modify: `c:/OpenADS/tests/unit/abi_write_smoke_test.cpp`

- [ ] **Step 1: Write the test**

Replace `c:/OpenADS/tests/unit/abi_write_smoke_test.cpp`:

```cpp
#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path make_empty_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    fs::remove(p);
    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 0;
    hdr[8]  = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 4;       hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C'; fd[16] = 4;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("ABI write smoke: append two rows, lock/unlock, mark deleted, read back") {
    const auto dir = fs::temp_directory_path() / "openads_m2_abi_w";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_empty_dbf(dir, "data.dbf");

    ADSHANDLE hConn = 0;
    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 leaf[64] = "data.dbf";
    REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    REQUIRE(AdsAppendRecord(hTable) == 0);
    UNSIGNED8 fld[64] = "TAG";
    UNSIGNED8 val1[8] = "ABC";
    REQUIRE(AdsSetString(hTable, fld, val1, 3) == 0);

    REQUIRE(AdsAppendRecord(hTable) == 0);
    UNSIGNED8 val2[8] = "WXYZ";
    REQUIRE(AdsSetString(hTable, fld, val2, 4) == 0);

    REQUIRE(AdsLockRecord(hTable, 1) == 0);
    REQUIRE(AdsUnlockRecord(hTable, 1) == 0);

    REQUIRE(AdsDeleteRecord(hTable) == 0);   // recno still 2
    UNSIGNED16 deleted = 0;
    REQUIRE(AdsIsRecordDeleted(hTable, &deleted) == 0);
    CHECK(deleted == 1);
    REQUIRE(AdsRecallRecord(hTable) == 0);
    REQUIRE(AdsIsRecordDeleted(hTable, &deleted) == 0);
    CHECK(deleted == 0);

    REQUIRE(AdsFlushFileBuffers(hTable) == 0);
    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);

    // Reopen and verify.
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    UNSIGNED32 count = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &count) == 0);
    CHECK(count == 2);

    REQUIRE(AdsGotoTop(hTable) == 0);
    UNSIGNED8 buf[16] = {0};
    UNSIGNED32 cap = sizeof(buf);
    REQUIRE(AdsGetField(hTable, fld, buf, &cap, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "ABC");

    REQUIRE(AdsSkip(hTable, 1) == 0);
    cap = sizeof(buf);
    std::memset(buf, 0, sizeof(buf));
    REQUIRE(AdsGetField(hTable, fld, buf, &cap, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "WXYZ");

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);

    fs::remove_all(dir, ec);
}
```

- [ ] **Step 2: Build + run**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 1 new test case passes.

- [ ] **Step 3: Commit**

```
git add tests/unit/abi_write_smoke_test.cpp
git commit -m "test(abi): end-to-end write smoke (append / set / lock / delete)"
```

---

## Task 9: README + tag m2-done

**Files:**
- Modify: `c:/OpenADS/README.md`

- [ ] **Step 1: Mark M2 done**

Replace the M2 row in the milestone table:

```markdown
| **M2 — DBF write + LockMgr** | [`2026-05-03-openads-m2-dbf-write-lock.md`](docs/superpowers/plans/2026-05-03-openads-m2-dbf-write-lock.md) | **Done.** Append / update / delete on CDX- and NTX-typed DBFs, `LockMgr` Compatible-mode byte ranges (NTX `1_000_000_000`, CDX `0x7FFFFFFE - recno`), single-process integrity tests. No pack / zap (M3), no memo (M4), no TPS (M5). |
```

- [ ] **Step 2: Commit, tag, push**

```
git add README.md
git commit -m "docs: mark M2 milestone done"
git tag m2-done
git push origin main --tags
```

---

## Done

At the end of M2:

- The L1 ABI gains 14 entry points covering write and lock surfaces.
- A doctest harness writes two rows through the ABI, locks/unlocks, deletes, recalls, flushes, then reopens and reads back the data.
- `LockMgr` returns the documented Compatible-mode ranges for both NTX and CDX schemes, exercised by unit tests.
- The repository is ready for M3 (CDX / NTX index read+write, pack / zap, AOF basics).
