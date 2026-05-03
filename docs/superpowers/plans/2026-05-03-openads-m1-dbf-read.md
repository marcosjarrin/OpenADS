# OpenADS — M1 DBF Read Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Open a CDX-typed DBF table read-only via the ACE C ABI, walk it with `AdsGotoTop` / `AdsSkip`, and read scalar field values with `AdsGetField`. End-to-end test: a Harbour-style sequence of L1 entry-point calls returns the same data the fixture writes byte-for-byte.

**Architecture:** L1 ABI thunks → L2 `Connection` / `HandleRegistry` → L4 `Table` and `Cursor` over a fresh `CdxDriver` that internally reuses a `dbf_common` parser shared with future NTX / VFP drivers. Read-only path only: no writes, no `.cdx` index reading (M3), no memo (M4). Fixtures are produced programmatically through the L5 `File` abstraction so the suite stays hermetic.

**Tech Stack:** Same as M0 (C++17, CMake, doctest). No new third-party dependencies.

---

## File structure for this milestone

Created in M1:

```
OpenADS/
├── include/openads/
│   └── ace.h                      # public ACE header subset for M1
├── src/
│   ├── abi/
│   │   ├── ace_types.h            # UNSIGNED8/16/32, ADSHANDLE, AE_*
│   │   ├── ace_exports.cpp        # extern "C" thunks for the M1 subset
│   │   ├── last_error.h
│   │   ├── last_error.cpp         # thread-local AdsGetLastError state
│   │   ├── charset.h
│   │   └── charset.cpp            # OEM/ANSI passthrough (real conv in M4)
│   ├── session/
│   │   ├── handle_registry.h
│   │   ├── handle_registry.cpp
│   │   ├── connection.h
│   │   └── connection.cpp
│   ├── engine/
│   │   ├── table.h
│   │   ├── table.cpp
│   │   ├── cursor.h
│   │   └── cursor.cpp
│   └── drivers/
│       ├── driver_trait.h         # minimal: open/close/read_record_raw
│       ├── dbf_common.h
│       ├── dbf_common.cpp         # header + field-descriptor parsers
│       └── cdx/
│           ├── cdx_driver.h
│           └── cdx_driver.cpp     # wraps dbf_common, marks ADS_CDX
└── tests/unit/
    ├── dbf_header_test.cpp
    ├── dbf_field_test.cpp
    ├── dbf_record_test.cpp
    ├── engine_table_test.cpp
    ├── engine_cursor_test.cpp
    ├── session_handle_registry_test.cpp
    ├── session_connection_test.cpp
    └── abi_smoke_test.cpp
```

Boundaries:

- `dbf_common` owns DBF header / field / record parsing. Future NTX / VFP drivers reuse it.
- `cdx/cdx_driver` is a thin wrapper that delegates everything to `dbf_common` and only labels itself with the ACE table-type constant.
- `Table` exposes a read-only navigation surface (`goto_top` / `goto_bottom` / `skip` / `recno` / `eof` / `bof` / `field_count` / `field_descriptor` / `read_field`). It does not know about ACE types — that translation lives in L1.
- `Cursor` is a thin forward-only view over a `Table` (single one per table here; SQL-driven cursors come in M7).
- `HandleRegistry` is a thread-safe slot table mapping `ADSHANDLE` → tagged pointer (`Connection*` / `Table*` / `Cursor*`).
- `ace_exports.cpp` is the only file that includes both `<openads/ace.h>` and the internal C++ headers; it does the marshalling.

---

## Task 1: Wire new module subdirectories into the build

**Files:**
- Modify: `c:/OpenADS/src/CMakeLists.txt`
- Modify: `c:/OpenADS/tests/CMakeLists.txt`

- [ ] **Step 1: Update `src/CMakeLists.txt` to compile the new modules**

Replace the contents of `c:/OpenADS/src/CMakeLists.txt` with:

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
    drivers/dbf_common.cpp
    drivers/cdx/cdx_driver.cpp
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

- [ ] **Step 2: Update `tests/CMakeLists.txt` to register the new test files**

Replace the contents of `c:/OpenADS/tests/CMakeLists.txt` with:

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
    unit/engine_table_test.cpp
    unit/engine_cursor_test.cpp
    unit/session_handle_registry_test.cpp
    unit/session_connection_test.cpp
    unit/abi_smoke_test.cpp
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

- [ ] **Step 3: Stub all new files so the build passes**

For each test file, write the placeholder line:

```cpp
#include "doctest.h"
```

- `c:/OpenADS/tests/unit/dbf_header_test.cpp`
- `c:/OpenADS/tests/unit/dbf_field_test.cpp`
- `c:/OpenADS/tests/unit/dbf_record_test.cpp`
- `c:/OpenADS/tests/unit/engine_table_test.cpp`
- `c:/OpenADS/tests/unit/engine_cursor_test.cpp`
- `c:/OpenADS/tests/unit/session_handle_registry_test.cpp`
- `c:/OpenADS/tests/unit/session_connection_test.cpp`
- `c:/OpenADS/tests/unit/abi_smoke_test.cpp`

For each new source file, write the placeholder line:

```cpp
// placeholder, real content lands in a later task
```

- `c:/OpenADS/src/abi/ace_exports.cpp`
- `c:/OpenADS/src/abi/last_error.cpp`
- `c:/OpenADS/src/abi/charset.cpp`
- `c:/OpenADS/src/session/handle_registry.cpp`
- `c:/OpenADS/src/session/connection.cpp`
- `c:/OpenADS/src/engine/table.cpp`
- `c:/OpenADS/src/engine/cursor.cpp`
- `c:/OpenADS/src/drivers/dbf_common.cpp`
- `c:/OpenADS/src/drivers/cdx/cdx_driver.cpp`

- [ ] **Step 4: Configure and build to verify the wiring**

Run:

```
cmake --preset default
cmake --build build/default --config Release
```

Expected: success.

- [ ] **Step 5: Run tests to confirm prior coverage still passes**

Run:

```
ctest --preset default -C Release --output-on-failure
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 27 cases pass (same as M0 closing state).

- [ ] **Step 6: Commit**

```
git add src/CMakeLists.txt tests/CMakeLists.txt src/abi src/session src/engine src/drivers tests/unit/dbf_header_test.cpp tests/unit/dbf_field_test.cpp tests/unit/dbf_record_test.cpp tests/unit/engine_table_test.cpp tests/unit/engine_cursor_test.cpp tests/unit/session_handle_registry_test.cpp tests/unit/session_connection_test.cpp tests/unit/abi_smoke_test.cpp
git commit -m "build: wire M1 module skeleton into CMake"
```

---

## Task 2: DBF header parser

**Files:**
- Create: `c:/OpenADS/src/drivers/dbf_common.h`
- Modify: `c:/OpenADS/src/drivers/dbf_common.cpp`
- Modify: `c:/OpenADS/tests/unit/dbf_header_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/dbf_header_test.cpp`:

```cpp
#include "doctest.h"
#include "drivers/dbf_common.h"

#include <array>
#include <cstdint>

using openads::drivers::DbfHeader;
using openads::drivers::parse_dbf_header;

namespace {

// Minimal valid 32-byte DBF header for version 0x03 (Clipper / dBase III).
//   byte  0 : version
//   bytes 1-3 : last update YY MM DD
//   bytes 4-7 : record count (uint32 little endian)
//   bytes 8-9 : header length (uint16 little endian)
//   bytes 10-11 : record length (uint16 little endian)
//   bytes 12-31 : reserved / flags
std::array<std::uint8_t, 32> sample_header() {
    std::array<std::uint8_t, 32> h{};
    h[0]  = 0x03;
    h[1]  = 124; // year offset 2024
    h[2]  = 1;
    h[3]  = 31;
    // record count = 5
    h[4]  = 0x05; h[5] = 0; h[6] = 0; h[7] = 0;
    // header length = 0x41 (65 = 32 + 32 + 1 terminator) — single field
    h[8]  = 0x41; h[9] = 0;
    // record length = 11 (10-char field + 1 deletion byte)
    h[10] = 0x0B; h[11] = 0;
    return h;
}

} // namespace

TEST_CASE("DBF header parser extracts version, recno count, sizes") {
    auto bytes = sample_header();
    auto parsed = parse_dbf_header(bytes.data(), bytes.size());
    REQUIRE(parsed.has_value());
    DbfHeader h = parsed.value();
    CHECK(h.version       == 0x03);
    CHECK(h.record_count  == 5);
    CHECK(h.header_length == 0x41);
    CHECK(h.record_length == 0x0B);
    CHECK(h.last_update_year  == 2024);
    CHECK(h.last_update_month == 1);
    CHECK(h.last_update_day   == 31);
}

TEST_CASE("DBF header parser rejects buffers shorter than 32 bytes") {
    std::array<std::uint8_t, 16> too_small{};
    auto parsed = parse_dbf_header(too_small.data(), too_small.size());
    CHECK_FALSE(parsed.has_value());
}

TEST_CASE("DBF header parser maps version 0x30 to VFP family") {
    auto bytes = sample_header();
    bytes[0] = 0x30;
    auto parsed = parse_dbf_header(bytes.data(), bytes.size());
    REQUIRE(parsed.has_value());
    CHECK(parsed.value().family == openads::drivers::DbfFamily::Vfp);
}

TEST_CASE("DBF header parser maps version 0x03 to Clipper family") {
    auto bytes = sample_header();
    auto parsed = parse_dbf_header(bytes.data(), bytes.size());
    REQUIRE(parsed.has_value());
    CHECK(parsed.value().family == openads::drivers::DbfFamily::Clipper);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `drivers/dbf_common.h` not found.

- [ ] **Step 3: Write `drivers/dbf_common.h`**

```cpp
#pragma once

#include "util/result.h"

#include <cstddef>
#include <cstdint>

namespace openads::drivers {

enum class DbfFamily {
    Clipper,    // 0x03 (no memo) / 0x83 (with memo)
    Vfp,        // 0x30 / 0x31
    Unknown
};

struct DbfHeader {
    std::uint8_t  version          = 0;
    std::uint16_t last_update_year = 0;
    std::uint8_t  last_update_month = 0;
    std::uint8_t  last_update_day  = 0;
    std::uint32_t record_count     = 0;
    std::uint16_t header_length    = 0;
    std::uint16_t record_length    = 0;
    DbfFamily     family           = DbfFamily::Unknown;
};

util::Result<DbfHeader> parse_dbf_header(const std::uint8_t* data,
                                         std::size_t size);

} // namespace openads::drivers
```

- [ ] **Step 4: Implement `drivers/dbf_common.cpp`**

Replace the placeholder with:

```cpp
#include "drivers/dbf_common.h"

namespace openads::drivers {

namespace {

DbfFamily classify(std::uint8_t version) {
    switch (version) {
        case 0x03: case 0x83:
            return DbfFamily::Clipper;
        case 0x30: case 0x31: case 0x32:
            return DbfFamily::Vfp;
        default:
            return DbfFamily::Unknown;
    }
}

std::uint16_t read_u16_le(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1] << 8);
}

std::uint32_t read_u32_le(const std::uint8_t* p) {
    return  static_cast<std::uint32_t>(p[0])        |
           (static_cast<std::uint32_t>(p[1]) << 8)  |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

} // namespace

util::Result<DbfHeader> parse_dbf_header(const std::uint8_t* data,
                                         std::size_t size) {
    if (size < 32) {
        return util::Error{5103, 0, "DBF header smaller than 32 bytes", ""};
    }
    DbfHeader h;
    h.version = data[0];
    // YY in DBF header is years since 1900. Apply that base.
    h.last_update_year  = static_cast<std::uint16_t>(1900 + data[1]);
    h.last_update_month = data[2];
    h.last_update_day   = data[3];
    h.record_count      = read_u32_le(data + 4);
    h.header_length     = read_u16_le(data + 8);
    h.record_length     = read_u16_le(data + 10);
    h.family            = classify(h.version);
    return h;
}

} // namespace openads::drivers
```

- [ ] **Step 5: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 4 new test cases pass alongside the previous 27.

- [ ] **Step 6: Commit**

```
git add src/drivers/dbf_common.h src/drivers/dbf_common.cpp tests/unit/dbf_header_test.cpp
git commit -m "feat(drivers): DBF header parser shared by CDX/NTX/VFP"
```

---

## Task 3: DBF field descriptors parser

**Files:**
- Modify: `c:/OpenADS/src/drivers/dbf_common.h`
- Modify: `c:/OpenADS/src/drivers/dbf_common.cpp`
- Modify: `c:/OpenADS/tests/unit/dbf_field_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/dbf_field_test.cpp`:

```cpp
#include "doctest.h"
#include "drivers/dbf_common.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

using openads::drivers::DbfField;
using openads::drivers::DbfFieldType;
using openads::drivers::parse_dbf_fields;

namespace {

// Build an N-field descriptor block of 32 bytes per field plus the
// trailing 0x0D terminator. Each descriptor:
//   bytes 0-10 : ASCII name padded with NUL
//   byte 11    : ASCII type letter
//   bytes 12-15: field data offset (uint32 LE) — VFP only, ignored
//   byte 16    : field length
//   byte 17    : decimal places
//   bytes 18-31: reserved
std::vector<std::uint8_t> build_descriptors(
        const std::vector<std::tuple<const char*, char, std::uint8_t,
                                     std::uint8_t>>& fields) {
    std::vector<std::uint8_t> out(fields.size() * 32 + 1, 0);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        std::uint8_t* slot = out.data() + i * 32;
        const auto& [name, type, len, dec] = fields[i];
        std::strncpy(reinterpret_cast<char*>(slot), name, 11);
        slot[11] = static_cast<std::uint8_t>(type);
        slot[16] = len;
        slot[17] = dec;
    }
    out.back() = 0x0D;
    return out;
}

} // namespace

TEST_CASE("Field parser reads name, type, length, decimals") {
    auto buf = build_descriptors({
        {"NAME",      'C', 20, 0},
        {"BALANCE",   'N', 12, 2},
        {"BORN",      'D',  8, 0},
        {"ACTIVE",    'L',  1, 0},
    });
    auto parsed = parse_dbf_fields(buf.data(), buf.size());
    REQUIRE(parsed.has_value());
    auto fields = parsed.value();
    REQUIRE(fields.size() == 4);

    CHECK(fields[0].name == "NAME");
    CHECK(fields[0].type == DbfFieldType::Character);
    CHECK(fields[0].length == 20);
    CHECK(fields[0].decimals == 0);

    CHECK(fields[1].name == "BALANCE");
    CHECK(fields[1].type == DbfFieldType::Numeric);
    CHECK(fields[1].length == 12);
    CHECK(fields[1].decimals == 2);

    CHECK(fields[2].type == DbfFieldType::Date);
    CHECK(fields[2].length == 8);

    CHECK(fields[3].type == DbfFieldType::Logical);
    CHECK(fields[3].length == 1);
}

TEST_CASE("Field parser stops at the 0x0D terminator") {
    auto buf = build_descriptors({
        {"ONLY", 'C', 5, 0},
    });
    auto parsed = parse_dbf_fields(buf.data(), buf.size());
    REQUIRE(parsed.has_value());
    CHECK(parsed.value().size() == 1);
}

TEST_CASE("Field parser flags unknown types as Unknown but still records them") {
    auto buf = build_descriptors({
        {"WEIRD", 'Q', 3, 0},
    });
    auto parsed = parse_dbf_fields(buf.data(), buf.size());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value().size() == 1);
    CHECK(parsed.value()[0].type == DbfFieldType::Unknown);
    CHECK(parsed.value()[0].raw_type == 'Q');
}

TEST_CASE("Field parser computes record offset for each field") {
    auto buf = build_descriptors({
        {"A", 'C', 5, 0},
        {"B", 'C', 7, 0},
        {"C", 'N', 3, 0},
    });
    auto parsed = parse_dbf_fields(buf.data(), buf.size());
    REQUIRE(parsed.has_value());
    auto fields = parsed.value();
    CHECK(fields[0].record_offset == 1);          // 1 = past deletion byte
    CHECK(fields[1].record_offset == 1 + 5);
    CHECK(fields[2].record_offset == 1 + 5 + 7);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `parse_dbf_fields` not declared.

- [ ] **Step 3: Append to `drivers/dbf_common.h`**

Open `c:/OpenADS/src/drivers/dbf_common.h` and add the following just before the closing namespace brace:

```cpp
enum class DbfFieldType {
    Character,
    Numeric,
    Float,
    Date,
    DateTime,
    Logical,
    Memo,
    Integer,    // VFP I (4-byte int)
    Currency,   // VFP Y
    Double,     // VFP B
    Unknown
};

struct DbfField {
    std::string   name;
    DbfFieldType  type          = DbfFieldType::Unknown;
    char          raw_type      = '\0';
    std::uint8_t  length        = 0;
    std::uint8_t  decimals      = 0;
    std::uint16_t record_offset = 0; // includes the leading deletion byte
};

util::Result<std::vector<DbfField>>
parse_dbf_fields(const std::uint8_t* data, std::size_t size);
```

Also add the corresponding includes at the top of `dbf_common.h`:

```cpp
#include <string>
#include <vector>
```

- [ ] **Step 4: Append to `drivers/dbf_common.cpp`**

Append the implementation:

```cpp
namespace openads::drivers {

namespace {

DbfFieldType classify_field(char raw) {
    switch (raw) {
        case 'C': return DbfFieldType::Character;
        case 'N': return DbfFieldType::Numeric;
        case 'F': return DbfFieldType::Float;
        case 'D': return DbfFieldType::Date;
        case 'T': return DbfFieldType::DateTime;
        case 'L': return DbfFieldType::Logical;
        case 'M': return DbfFieldType::Memo;
        case 'I': return DbfFieldType::Integer;
        case 'Y': return DbfFieldType::Currency;
        case 'B': return DbfFieldType::Double;
        default:  return DbfFieldType::Unknown;
    }
}

} // namespace

util::Result<std::vector<DbfField>>
parse_dbf_fields(const std::uint8_t* data, std::size_t size) {
    std::vector<DbfField> out;
    std::uint16_t offset = 1; // skip leading deletion byte

    std::size_t pos = 0;
    while (pos + 32 <= size) {
        if (data[pos] == 0x0D) break;
        DbfField f;
        // name: NUL-terminated within first 11 bytes
        const char* raw_name = reinterpret_cast<const char*>(data + pos);
        std::size_t name_len = 0;
        while (name_len < 11 && raw_name[name_len] != '\0') ++name_len;
        f.name.assign(raw_name, name_len);
        f.raw_type      = static_cast<char>(data[pos + 11]);
        f.type          = classify_field(f.raw_type);
        f.length        = data[pos + 16];
        f.decimals      = data[pos + 17];
        f.record_offset = offset;
        offset = static_cast<std::uint16_t>(offset + f.length);
        out.push_back(std::move(f));
        pos += 32;
    }
    return out;
}

} // namespace openads::drivers
```

Note that the new namespace block sits in the same translation unit as the
existing one — they will be merged by the compiler. Keep the existing block
intact above this addition.

- [ ] **Step 5: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 4 new test cases pass.

- [ ] **Step 6: Commit**

```
git add src/drivers/dbf_common.h src/drivers/dbf_common.cpp tests/unit/dbf_field_test.cpp
git commit -m "feat(drivers): DBF field-descriptor parser with record offsets"
```

---

## Task 4: Record reader and field decoder

**Files:**
- Modify: `c:/OpenADS/src/drivers/dbf_common.h`
- Modify: `c:/OpenADS/src/drivers/dbf_common.cpp`
- Modify: `c:/OpenADS/tests/unit/dbf_record_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/dbf_record_test.cpp`:

```cpp
#include "doctest.h"
#include "drivers/dbf_common.h"

#include <cstdint>
#include <string>
#include <vector>

using openads::drivers::DbfField;
using openads::drivers::DbfFieldType;
using openads::drivers::decode_field;
using openads::drivers::record_is_deleted;

TEST_CASE("Character field decodes ASCII trimmed of trailing spaces") {
    DbfField f;
    f.type          = DbfFieldType::Character;
    f.length        = 10;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        'C', 'l', 'i', 'p', 'p', 'e', 'r', ' ', ' ', ' '};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_string == "Clipper");
}

TEST_CASE("Numeric field decodes an integer-looking value") {
    DbfField f;
    f.type          = DbfFieldType::Numeric;
    f.length        = 6;
    f.decimals      = 0;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        ' ', ' ', ' ', '4', '2', '0'};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_double == doctest::Approx(420.0));
}

TEST_CASE("Numeric field decodes a fractional value") {
    DbfField f;
    f.type          = DbfFieldType::Numeric;
    f.length        = 7;
    f.decimals      = 2;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        ' ', ' ', '3', '.', '1', '4', ' '};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_double == doctest::Approx(3.14));
}

TEST_CASE("Logical field decodes T/F/Y/N") {
    DbfField f;
    f.type          = DbfFieldType::Logical;
    f.length        = 1;
    f.record_offset = 1;
    for (auto [byte, expected] : std::initializer_list<std::pair<char, bool>>{
             {'T', true}, {'t', true}, {'Y', true}, {'y', true},
             {'F', false}, {'f', false}, {'N', false}, {'n', false},
             {'?', false}, {' ', false}}) {
        std::vector<std::uint8_t> rec = {' ',
            static_cast<std::uint8_t>(byte)};
        auto v = decode_field(f, rec.data(), rec.size());
        REQUIRE(v.has_value());
        CHECK(v.value().as_bool == expected);
    }
}

TEST_CASE("Date field decodes YYYYMMDD ASCII") {
    DbfField f;
    f.type          = DbfFieldType::Date;
    f.length        = 8;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        '2', '0', '2', '6', '0', '5', '0', '3'};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_string == "20260503");
}

TEST_CASE("Memo field returns an empty placeholder in M1") {
    DbfField f;
    f.type          = DbfFieldType::Memo;
    f.length        = 10;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_string.empty());
}

TEST_CASE("record_is_deleted reads the deletion byte") {
    std::vector<std::uint8_t> alive   = {' ', 'a', 'b', 'c'};
    std::vector<std::uint8_t> deleted = {'*', 'a', 'b', 'c'};
    CHECK_FALSE(record_is_deleted(alive.data(),   alive.size()));
    CHECK      (record_is_deleted(deleted.data(), deleted.size()));
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `decode_field` not declared.

- [ ] **Step 3: Extend `drivers/dbf_common.h`**

Append before the closing namespace brace:

```cpp
struct DbfFieldValue {
    std::string  as_string;
    double       as_double = 0.0;
    bool         as_bool   = false;
    bool         is_null   = false;
};

util::Result<DbfFieldValue> decode_field(const DbfField& field,
                                         const std::uint8_t* record_buf,
                                         std::size_t record_size);

bool record_is_deleted(const std::uint8_t* record_buf,
                       std::size_t record_size) noexcept;
```

- [ ] **Step 4: Extend `drivers/dbf_common.cpp`**

Append:

```cpp
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace openads::drivers {

namespace {

std::string make_string(const std::uint8_t* p, std::size_t n) {
    std::string s(reinterpret_cast<const char*>(p), n);
    // trim trailing spaces (DBF convention for fixed-width chars)
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

double parse_numeric(const std::uint8_t* p, std::size_t n) {
    // DBF numerics are right-aligned ASCII with optional sign and dot.
    // Copy into a NUL-terminated stack buffer for strtod.
    char tmp[64];
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    std::memcpy(tmp, p, n);
    tmp[n] = '\0';
    char* end = nullptr;
    return std::strtod(tmp, &end);
}

} // namespace

util::Result<DbfFieldValue> decode_field(const DbfField& field,
                                         const std::uint8_t* record_buf,
                                         std::size_t record_size) {
    DbfFieldValue v;
    if (field.record_offset + field.length > record_size) {
        return util::Error{5000, 0, "field range past record buffer", ""};
    }
    const std::uint8_t* p = record_buf + field.record_offset;

    switch (field.type) {
        case DbfFieldType::Character:
            v.as_string = make_string(p, field.length);
            break;

        case DbfFieldType::Numeric:
        case DbfFieldType::Float:
            v.as_double = parse_numeric(p, field.length);
            v.as_string = make_string(p, field.length);
            break;

        case DbfFieldType::Date:
        case DbfFieldType::DateTime:
            v.as_string = make_string(p, field.length);
            break;

        case DbfFieldType::Logical: {
            char c = static_cast<char>(p[0]);
            v.as_bool   = (c == 'T' || c == 't' || c == 'Y' || c == 'y');
            v.as_string = std::string(1, c);
            break;
        }

        case DbfFieldType::Memo:
            // M1 deliberately does not load memo blocks; they land in M4.
            v.as_string.clear();
            break;

        case DbfFieldType::Integer:
        case DbfFieldType::Currency:
        case DbfFieldType::Double:
        case DbfFieldType::Unknown:
            v.as_string = make_string(p, field.length);
            break;
    }
    return v;
}

bool record_is_deleted(const std::uint8_t* record_buf,
                       std::size_t record_size) noexcept {
    if (record_size == 0) return false;
    return record_buf[0] == '*';
}

} // namespace openads::drivers
```

- [ ] **Step 5: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 7 new test cases pass.

- [ ] **Step 6: Commit**

```
git add src/drivers/dbf_common.h src/drivers/dbf_common.cpp tests/unit/dbf_record_test.cpp
git commit -m "feat(drivers): DBF record decoder for C/N/F/D/T/L (memo deferred)"
```

---

## Task 5: HandleRegistry

**Files:**
- Create: `c:/OpenADS/src/session/handle_registry.h`
- Modify: `c:/OpenADS/src/session/handle_registry.cpp`
- Modify: `c:/OpenADS/tests/unit/session_handle_registry_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/session_handle_registry_test.cpp`:

```cpp
#include "doctest.h"
#include "session/handle_registry.h"

using openads::session::HandleKind;
using openads::session::HandleRegistry;

TEST_CASE("HandleRegistry assigns distinct, non-zero handles") {
    HandleRegistry reg;
    int a = 1, b = 2;
    auto h1 = reg.register_object(HandleKind::Connection, &a);
    auto h2 = reg.register_object(HandleKind::Connection, &b);
    CHECK(h1 != 0);
    CHECK(h2 != 0);
    CHECK(h1 != h2);
}

TEST_CASE("HandleRegistry resolves and respects the kind tag") {
    HandleRegistry reg;
    int conn_obj = 0;
    int tbl_obj  = 0;
    auto hc = reg.register_object(HandleKind::Connection, &conn_obj);
    auto ht = reg.register_object(HandleKind::Table,      &tbl_obj);

    CHECK(reg.lookup<int>(hc, HandleKind::Connection) == &conn_obj);
    CHECK(reg.lookup<int>(ht, HandleKind::Table)      == &tbl_obj);

    // Wrong-kind lookup returns nullptr.
    CHECK(reg.lookup<int>(hc, HandleKind::Table) == nullptr);
}

TEST_CASE("HandleRegistry returns nullptr for unknown handles") {
    HandleRegistry reg;
    CHECK(reg.lookup<int>(0,        HandleKind::Connection) == nullptr);
    CHECK(reg.lookup<int>(99999999, HandleKind::Connection) == nullptr);
}

TEST_CASE("HandleRegistry releases handles") {
    HandleRegistry reg;
    int o = 0;
    auto h = reg.register_object(HandleKind::Table, &o);
    REQUIRE(reg.lookup<int>(h, HandleKind::Table) != nullptr);
    reg.release(h);
    CHECK(reg.lookup<int>(h, HandleKind::Table) == nullptr);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `session/handle_registry.h` not found.

- [ ] **Step 3: Implement `session/handle_registry.h`**

```cpp
#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace openads::session {

enum class HandleKind {
    None       = 0,
    Connection = 1,
    Table      = 2,
    Cursor     = 3,
    Statement  = 4
};

using Handle = std::uint64_t;

class HandleRegistry {
public:
    Handle register_object(HandleKind kind, void* ptr);
    void   release(Handle h);

    template <class T>
    T* lookup(Handle h, HandleKind kind) const {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = slots_.find(h);
        if (it == slots_.end()) return nullptr;
        if (it->second.kind != kind) return nullptr;
        return static_cast<T*>(it->second.ptr);
    }

private:
    struct Slot { HandleKind kind = HandleKind::None; void* ptr = nullptr; };

    mutable std::mutex                  mu_;
    std::unordered_map<Handle, Slot>    slots_;
    Handle                              next_ = 1;
};

} // namespace openads::session
```

- [ ] **Step 4: Implement `session/handle_registry.cpp`**

Replace the placeholder with:

```cpp
#include "session/handle_registry.h"

namespace openads::session {

Handle HandleRegistry::register_object(HandleKind kind, void* ptr) {
    std::lock_guard<std::mutex> lk(mu_);
    Handle h = next_++;
    slots_[h] = Slot{kind, ptr};
    return h;
}

void HandleRegistry::release(Handle h) {
    std::lock_guard<std::mutex> lk(mu_);
    slots_.erase(h);
}

} // namespace openads::session
```

- [ ] **Step 5: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 4 new test cases pass.

- [ ] **Step 6: Commit**

```
git add src/session/handle_registry.h src/session/handle_registry.cpp tests/unit/session_handle_registry_test.cpp
git commit -m "feat(session): kind-tagged HandleRegistry with thread-safe slot map"
```

---

## Task 6: Engine Table (read-only navigation)

**Files:**
- Create: `c:/OpenADS/src/engine/table.h`
- Modify: `c:/OpenADS/src/engine/table.cpp`
- Create: `c:/OpenADS/src/drivers/driver_trait.h`
- Modify: `c:/OpenADS/src/drivers/cdx/cdx_driver.h` (create)
- Modify: `c:/OpenADS/src/drivers/cdx/cdx_driver.cpp`
- Modify: `c:/OpenADS/tests/unit/engine_table_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/engine_table_test.cpp`:

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
using openads::engine::Table;
using openads::engine::TableType;

namespace {

// Build a tiny DBF on disk:
//   - version 0x03
//   - 3 records, single 5-char field "NAME"
fs::path make_fixture(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("openads_m1_") + tag);
    fs::remove(p);

    std::vector<std::uint8_t> file;

    // Header (32 bytes)
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[1]  = 124; hdr[2] = 1; hdr[3] = 31;
    hdr[4]  = 3; hdr[5] = 0; hdr[6] = 0; hdr[7] = 0;       // recno count
    hdr[8]  = 32 + 32 + 1; hdr[9] = 0;                     // header length
    hdr[10] = 1 + 5; hdr[11] = 0;                          // record length
    file.insert(file.end(), hdr.begin(), hdr.end());

    // One field descriptor (32 bytes) + 0x0D terminator
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "NAME", 11);
    fd[11] = 'C';
    fd[16] = 5;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);

    // 3 records of 6 bytes each (deletion byte + 5-char name)
    auto push_rec = [&](char d, const char* name) {
        file.push_back(static_cast<std::uint8_t>(d));
        for (int i = 0; i < 5; ++i) {
            file.push_back(static_cast<std::uint8_t>(
                i < static_cast<int>(std::strlen(name)) ? name[i] : ' '));
        }
    };
    push_rec(' ', "AAA");
    push_rec(' ', "BBBB");
    push_rec(' ', "CCCCC");

    // EOF marker (0x1A) — optional in spec, present in conformance fixtures
    file.push_back(0x1A);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("Table opens a CDX-typed DBF and reports counts") {
    auto p = make_fixture("table_counts");
    auto opened = Table::open(p.string(), TableType::Cdx);
    REQUIRE(opened.has_value());
    Table t = std::move(opened).value();

    CHECK(t.field_count()  == 1);
    CHECK(t.record_count() == 3);
    CHECK(t.field_descriptor(0).name == "NAME");

    fs::remove(p);
}

TEST_CASE("Table navigates top / skip / bottom and tracks BOF/EOF") {
    auto p = make_fixture("table_nav");
    auto opened = Table::open(p.string(), TableType::Cdx);
    REQUIRE(opened.has_value());
    Table t = std::move(opened).value();

    REQUIRE(t.goto_top().has_value());
    CHECK(t.recno() == 1);
    CHECK_FALSE(t.eof());
    CHECK_FALSE(t.bof());

    REQUIRE(t.skip(1).has_value());
    CHECK(t.recno() == 2);

    REQUIRE(t.skip(5).has_value());
    CHECK(t.eof());

    REQUIRE(t.goto_bottom().has_value());
    CHECK(t.recno() == 3);

    REQUIRE(t.skip(-10).has_value());
    CHECK(t.bof());

    fs::remove(p);
}

TEST_CASE("Table reads field values by index") {
    auto p = make_fixture("table_field");
    auto opened = Table::open(p.string(), TableType::Cdx);
    REQUIRE(opened.has_value());
    Table t = std::move(opened).value();

    REQUIRE(t.goto_top().has_value());
    auto v0 = t.read_field(0);
    REQUIRE(v0.has_value());
    CHECK(v0.value().as_string == "AAA");

    REQUIRE(t.skip(1).has_value());
    auto v1 = t.read_field(0);
    REQUIRE(v1.has_value());
    CHECK(v1.value().as_string == "BBBB");

    REQUIRE(t.skip(1).has_value());
    auto v2 = t.read_field(0);
    REQUIRE(v2.has_value());
    CHECK(v2.value().as_string == "CCCCC");

    fs::remove(p);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `engine/table.h` not found.

- [ ] **Step 3: Write `drivers/driver_trait.h`**

```cpp
#pragma once

#include "drivers/dbf_common.h"
#include "platform/file.h"
#include "util/result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openads::drivers {

// Minimal driver surface required by M1. Drivers will grow as later
// milestones add write, index, memo, encryption, and so on.
class IDriver {
public:
    virtual ~IDriver() = default;

    virtual util::Result<void>
        open(const std::string& path) = 0;

    virtual std::uint32_t record_count() const noexcept = 0;
    virtual std::uint16_t record_length() const noexcept = 0;
    virtual const std::vector<DbfField>& fields() const noexcept = 0;

    // Reads a single record's raw bytes (including the deletion byte).
    // recno is 1-based as in xBase. recno == 0 is invalid.
    virtual util::Result<std::vector<std::uint8_t>>
        read_record_raw(std::uint32_t recno) = 0;
};

} // namespace openads::drivers
```

- [ ] **Step 4: Write `drivers/cdx/cdx_driver.h`**

```cpp
#pragma once

#include "drivers/driver_trait.h"
#include "platform/file.h"

namespace openads::drivers::cdx {

class CdxDriver final : public IDriver {
public:
    util::Result<void> open(const std::string& path) override;

    std::uint32_t record_count() const noexcept override { return rec_count_; }
    std::uint16_t record_length() const noexcept override { return rec_len_; }
    const std::vector<DbfField>& fields() const noexcept override { return fields_; }

    util::Result<std::vector<std::uint8_t>>
        read_record_raw(std::uint32_t recno) override;

private:
    platform::File          file_;
    std::vector<DbfField>   fields_;
    std::uint32_t           rec_count_ = 0;
    std::uint16_t           rec_len_   = 0;
    std::uint16_t           hdr_len_   = 0;
};

} // namespace openads::drivers::cdx
```

- [ ] **Step 5: Replace `drivers/cdx/cdx_driver.cpp`**

```cpp
#include "drivers/cdx/cdx_driver.h"

#include <vector>

namespace openads::drivers::cdx {

util::Result<void> CdxDriver::open(const std::string& path) {
    auto fres = platform::File::open(path, platform::OpenMode::ReadOnly);
    if (!fres) return fres.error();
    file_ = std::move(fres).value();

    // Read the 32-byte header.
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

    // Read the field-descriptor block (header_length - 32 bytes).
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

} // namespace openads::drivers::cdx
```

- [ ] **Step 6: Write `engine/table.h`**

```cpp
#pragma once

#include "drivers/driver_trait.h"
#include "drivers/dbf_common.h"
#include "util/result.h"

#include <cstdint>
#include <memory>
#include <string>

namespace openads::engine {

enum class TableType { Cdx, Ntx, Adt, Vfp };

class Table {
public:
    Table() = default;
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;
    Table(Table&&) noexcept = default;
    Table& operator=(Table&&) noexcept = default;
    ~Table() = default;

    static util::Result<Table> open(const std::string& path, TableType type);

    std::uint16_t field_count() const noexcept;
    const drivers::DbfField& field_descriptor(std::uint16_t idx) const;

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

private:
    enum class State { Bof, Positioned, Eof };

    explicit Table(std::unique_ptr<drivers::IDriver> drv) noexcept
        : driver_(std::move(drv)) {}

    util::Result<void> ensure_record_loaded_();
    util::Result<void> load_record_(std::uint32_t recno);

    std::unique_ptr<drivers::IDriver> driver_;
    State                             state_  = State::Bof;
    std::uint32_t                     recno_  = 0;
    std::vector<std::uint8_t>         record_buf_;
};

} // namespace openads::engine
```

- [ ] **Step 7: Implement `engine/table.cpp`**

Replace the placeholder with:

```cpp
#include "engine/table.h"

#include "drivers/cdx/cdx_driver.h"

#include <utility>

namespace openads::engine {

util::Result<Table> Table::open(const std::string& path, TableType type) {
    std::unique_ptr<drivers::IDriver> drv;
    switch (type) {
        case TableType::Cdx:
            drv = std::make_unique<drivers::cdx::CdxDriver>();
            break;
        case TableType::Ntx:
        case TableType::Adt:
        case TableType::Vfp:
            return util::Error{5004, 0,
                               "table type not yet supported in M1", path};
    }
    if (auto r = drv->open(path); !r) return r.error();
    return Table{std::move(drv)};
}

std::uint16_t Table::field_count() const noexcept {
    return static_cast<std::uint16_t>(driver_->fields().size());
}

const drivers::DbfField& Table::field_descriptor(std::uint16_t idx) const {
    return driver_->fields().at(idx);
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

util::Result<void> Table::goto_top() {
    if (driver_->record_count() == 0) {
        state_ = State::Eof;
        recno_ = 0;
        return {};
    }
    return load_record_(1);
}

util::Result<void> Table::goto_bottom() {
    auto n = driver_->record_count();
    if (n == 0) {
        state_ = State::Eof;
        recno_ = 0;
        return {};
    }
    return load_record_(n);
}

util::Result<void> Table::goto_record(std::uint32_t recno) {
    if (recno == 0 || recno > driver_->record_count()) {
        state_ = State::Eof;
        recno_ = 0;
        return util::Error{5000, 0, "recno out of range", ""};
    }
    return load_record_(recno);
}

util::Result<void> Table::skip(std::int32_t delta) {
    auto n = driver_->record_count();
    if (n == 0) { state_ = State::Eof; recno_ = 0; return {}; }

    std::int64_t target = static_cast<std::int64_t>(recno_) + delta;
    if (state_ == State::Bof && delta > 0) target = delta;

    if (target < 1) {
        state_ = State::Bof;
        recno_ = 0;
        return {};
    }
    if (target > static_cast<std::int64_t>(n)) {
        state_ = State::Eof;
        recno_ = n + 1;
        return {};
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

} // namespace openads::engine
```

- [ ] **Step 8: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 3 new test cases pass.

- [ ] **Step 9: Commit**

```
git add src/drivers/driver_trait.h src/drivers/cdx/cdx_driver.h src/drivers/cdx/cdx_driver.cpp src/engine/table.h src/engine/table.cpp tests/unit/engine_table_test.cpp
git commit -m "feat(engine): read-only Table backed by CdxDriver over DBF"
```

---

## Task 7: Engine Cursor (forward iterator over a Table)

**Files:**
- Create: `c:/OpenADS/src/engine/cursor.h`
- Modify: `c:/OpenADS/src/engine/cursor.cpp`
- Modify: `c:/OpenADS/tests/unit/engine_cursor_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/engine_cursor_test.cpp`:

```cpp
#include "doctest.h"
#include "engine/cursor.h"
#include "engine/table.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using openads::engine::Cursor;
using openads::engine::Table;
using openads::engine::TableType;

namespace {

fs::path make_fixture() {
    auto p = fs::temp_directory_path() / "openads_m1_cursor.dbf";
    fs::remove(p);
    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03;
    hdr[4] = 2;                                // 2 records
    hdr[8] = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 4; hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C';
    fd[16] = 4;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    auto push_rec = [&](const char* name) {
        file.push_back(' ');
        for (int i = 0; i < 4; ++i) {
            file.push_back(static_cast<std::uint8_t>(
                i < static_cast<int>(std::strlen(name)) ? name[i] : ' '));
        }
    };
    push_rec("X");
    push_rec("YZ");
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("Cursor walks all live records once") {
    auto p = make_fixture();
    auto t = Table::open(p.string(), TableType::Cdx);
    REQUIRE(t.has_value());
    Table table = std::move(t).value();
    Cursor c(table);

    std::vector<std::string> seen;
    while (auto row = c.next()) {
        auto v = row->read_field(0);
        REQUIRE(v.has_value());
        seen.push_back(v.value().as_string);
    }
    CHECK(seen == std::vector<std::string>{"X", "YZ"});

    fs::remove(p);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `engine/cursor.h` not found.

- [ ] **Step 3: Implement `engine/cursor.h`**

```cpp
#pragma once

#include "engine/table.h"

#include <optional>

namespace openads::engine {

// Forward iteration view of a Table. The Cursor borrows the Table; the
// caller owns the Table lifetime. `next()` returns a non-owning pointer
// to the same Table positioned on the next live record, or std::nullopt
// at EOF.
class Cursor {
public:
    explicit Cursor(Table& t) noexcept : table_(&t) {}

    std::optional<Table*> next();

private:
    Table* table_;
    bool   started_ = false;
};

} // namespace openads::engine
```

- [ ] **Step 4: Implement `engine/cursor.cpp`**

Replace placeholder with:

```cpp
#include "engine/cursor.h"

namespace openads::engine {

std::optional<Table*> Cursor::next() {
    if (!started_) {
        started_ = true;
        if (!table_->goto_top()) return std::nullopt;
    } else {
        if (!table_->skip(1))    return std::nullopt;
    }
    if (table_->eof()) return std::nullopt;
    return table_;
}

} // namespace openads::engine
```

- [ ] **Step 5: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 1 new test case passes.

- [ ] **Step 6: Commit**

```
git add src/engine/cursor.h src/engine/cursor.cpp tests/unit/engine_cursor_test.cpp
git commit -m "feat(engine): forward-only Cursor over Table"
```

---

## Task 8: Session Connection

**Files:**
- Create: `c:/OpenADS/src/session/connection.h`
- Modify: `c:/OpenADS/src/session/connection.cpp`
- Modify: `c:/OpenADS/tests/unit/session_connection_test.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `c:/OpenADS/tests/unit/session_connection_test.cpp`:

```cpp
#include "doctest.h"
#include "session/connection.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using openads::engine::TableType;
using openads::session::Connection;

namespace {

fs::path tmp_dir(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("openads_m1_conn_") + tag);
    fs::remove_all(p);
    fs::create_directories(p);
    return p;
}

void write_minimal_dbf(const fs::path& p) {
    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03;
    hdr[4] = 1;
    hdr[8] = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 3; hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C';
    fd[16] = 3;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(' '); file.push_back('a'); file.push_back('b'); file.push_back('c');
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
}

} // namespace

TEST_CASE("Connection opens against a directory") {
    auto dir = tmp_dir("open");
    auto opened = Connection::open(dir.string());
    REQUIRE(opened.has_value());
    fs::remove_all(dir);
}

TEST_CASE("Connection opens a CDX-typed table by relative name") {
    auto dir = tmp_dir("table");
    write_minimal_dbf(dir / "data.dbf");

    auto opened = Connection::open(dir.string());
    REQUIRE(opened.has_value());
    Connection c = std::move(opened).value();

    auto th = c.open_table("data.dbf", TableType::Cdx);
    REQUIRE(th.has_value());
    auto* table = c.lookup_table(th.value());
    REQUIRE(table != nullptr);
    CHECK(table->field_count() == 1);
    CHECK(table->record_count() == 1);

    c.close_table(th.value());
    CHECK(c.lookup_table(th.value()) == nullptr);

    fs::remove_all(dir);
}
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run:

```
cmake --build build/default --config Release
```

Expected: compile error, `session/connection.h` not found.

- [ ] **Step 3: Write `session/connection.h`**

```cpp
#pragma once

#include "engine/table.h"
#include "session/handle_registry.h"
#include "util/result.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace openads::session {

class Connection {
public:
    Connection() = default;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept = default;
    Connection& operator=(Connection&&) noexcept = default;

    static util::Result<Connection> open(const std::string& data_dir);

    util::Result<Handle>
        open_table(const std::string& relative_path,
                   engine::TableType  type);

    void close_table(Handle h);

    engine::Table* lookup_table(Handle h);

    const std::string& data_dir() const noexcept { return data_dir_; }

private:
    std::string                                            data_dir_;
    HandleRegistry                                         registry_;
    std::unordered_map<Handle, std::unique_ptr<engine::Table>> tables_;
};

} // namespace openads::session
```

- [ ] **Step 4: Implement `session/connection.cpp`**

Replace placeholder with:

```cpp
#include "session/connection.h"

#include "platform/path.h"

#include <filesystem>
#include <utility>

namespace openads::session {

util::Result<Connection> Connection::open(const std::string& data_dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(data_dir, ec)) {
        return util::Error{5103, 0, "data directory not found", data_dir};
    }
    Connection c;
    c.data_dir_ = data_dir;
    return c;
}

util::Result<Handle> Connection::open_table(const std::string& relative_path,
                                            engine::TableType  type) {
    namespace fs = std::filesystem;
    fs::path full = fs::path(data_dir_) / relative_path;
    auto resolved = platform::resolve_case_insensitive(full.string());

    auto t = engine::Table::open(resolved, type);
    if (!t) return t.error();

    auto holder = std::make_unique<engine::Table>(std::move(t).value());
    engine::Table* raw = holder.get();
    Handle h = registry_.register_object(HandleKind::Table, raw);
    tables_.emplace(h, std::move(holder));
    return h;
}

void Connection::close_table(Handle h) {
    registry_.release(h);
    tables_.erase(h);
}

engine::Table* Connection::lookup_table(Handle h) {
    return registry_.lookup<engine::Table>(h, HandleKind::Table);
}

} // namespace openads::session
```

- [ ] **Step 5: Run tests to verify they pass**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 2 new test cases pass.

- [ ] **Step 6: Commit**

```
git add src/session/connection.h src/session/connection.cpp tests/unit/session_connection_test.cpp
git commit -m "feat(session): Connection with table registry over a data directory"
```

---

## Task 9: Public ACE header subset and ABI thunks

**Files:**
- Create: `c:/OpenADS/include/openads/ace.h`
- Create: `c:/OpenADS/src/abi/ace_types.h`
- Create: `c:/OpenADS/src/abi/last_error.h`
- Modify: `c:/OpenADS/src/abi/last_error.cpp`
- Create: `c:/OpenADS/src/abi/charset.h`
- Modify: `c:/OpenADS/src/abi/charset.cpp`
- Modify: `c:/OpenADS/src/abi/ace_exports.cpp`

- [ ] **Step 1: Write `include/openads/ace.h` (M1 subset)**

```cpp
#pragma once

// OpenADS ACE-compatible C ABI — phase 1, milestone M1 subset.
// See openads/error.h for AE_* error codes.

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  UNSIGNED8;
typedef uint16_t UNSIGNED16;
typedef uint32_t UNSIGNED32;
typedef int32_t  SIGNED32;
typedef uint64_t ADSHANDLE;

#define ADS_DEFAULT 0
#define ADS_NTX     1
#define ADS_CDX     2
#define ADS_ADT     3
#define ADS_VFP     4

#define ADS_LOCAL_SERVER  1
#define ADS_REMOTE_SERVER 2

UNSIGNED32 AdsConnect60     (UNSIGNED8* pucServer, UNSIGNED16 usServerType,
                              UNSIGNED8* pucUserName, UNSIGNED8* pucPassword,
                              UNSIGNED32 ulOptions, ADSHANDLE* phConnect);
UNSIGNED32 AdsDisconnect    (ADSHANDLE hConnect);

UNSIGNED32 AdsOpenTable     (ADSHANDLE  hConnect,
                              UNSIGNED8* pucName,
                              UNSIGNED8* pucAlias,
                              UNSIGNED16 usTableType,
                              UNSIGNED16 usCharType,
                              UNSIGNED16 usLockType,
                              UNSIGNED16 usCheckRights,
                              UNSIGNED16 usMode,
                              ADSHANDLE* phTable);
UNSIGNED32 AdsCloseTable    (ADSHANDLE hTable);

UNSIGNED32 AdsGotoTop       (ADSHANDLE hTable);
UNSIGNED32 AdsGotoBottom    (ADSHANDLE hTable);
UNSIGNED32 AdsSkip          (ADSHANDLE hTable, SIGNED32 lRows);
UNSIGNED32 AdsAtEOF         (ADSHANDLE hTable, UNSIGNED16* pbAtEnd);
UNSIGNED32 AdsAtBOF         (ADSHANDLE hTable, UNSIGNED16* pbAtBegin);

UNSIGNED32 AdsGetField      (ADSHANDLE  hTable, UNSIGNED8* pucField,
                              UNSIGNED8* pucBuf, UNSIGNED32* pulLen,
                              UNSIGNED16 usOption);
UNSIGNED32 AdsGetFieldName  (ADSHANDLE  hTable, UNSIGNED16 usFieldNum,
                              UNSIGNED8* pucBuf, UNSIGNED16* pusLen);
UNSIGNED32 AdsGetNumFields  (ADSHANDLE  hTable, UNSIGNED16* pusFields);
UNSIGNED32 AdsGetFieldType  (ADSHANDLE  hTable, UNSIGNED8* pucField,
                              UNSIGNED16* pusType);
UNSIGNED32 AdsGetFieldLength(ADSHANDLE  hTable, UNSIGNED8* pucField,
                              UNSIGNED32* pulLen);
UNSIGNED32 AdsGetRecordNum  (ADSHANDLE  hTable, UNSIGNED16 bFilterOption,
                              UNSIGNED32* pulRecordNum);
UNSIGNED32 AdsGetRecordCount(ADSHANDLE  hTable, UNSIGNED16 bFilterOption,
                              UNSIGNED32* pulRecordCount);

UNSIGNED32 AdsGetLastError  (UNSIGNED32* pulCode, UNSIGNED8* pucBuf,
                              UNSIGNED16* pusBufLen);

UNSIGNED32 AdsGetVersion    (UNSIGNED32* pulMajor, UNSIGNED32* pulMinor,
                              UNSIGNED32* pulLetter, UNSIGNED32* pulDesc);

#define ADS_FIELD_TYPE_CHAR       1
#define ADS_FIELD_TYPE_NUMERIC    2
#define ADS_FIELD_TYPE_LOGICAL    3
#define ADS_FIELD_TYPE_DATE       4
#define ADS_FIELD_TYPE_DATETIME   5
#define ADS_FIELD_TYPE_MEMO       6
#define ADS_FIELD_TYPE_INTEGER    7
#define ADS_FIELD_TYPE_DOUBLE     8
#define ADS_FIELD_TYPE_CURRENCY   9
#define ADS_FIELD_TYPE_UNKNOWN    99

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Write `src/abi/ace_types.h`**

```cpp
#pragma once

#include "openads/ace.h"
```

- [ ] **Step 3: Write `src/abi/last_error.h`**

```cpp
#pragma once

#include "util/result.h"

#include <cstdint>
#include <string>

namespace openads::abi {

void set_last_error(const util::Error& e) noexcept;
void clear_last_error() noexcept;

std::int32_t  last_error_code() noexcept;
const std::string& last_error_message() noexcept;

} // namespace openads::abi
```

- [ ] **Step 4: Replace `src/abi/last_error.cpp`**

```cpp
#include "abi/last_error.h"

namespace openads::abi {

namespace {
thread_local std::int32_t  g_code = 0;
thread_local std::string   g_msg;
} // namespace

void set_last_error(const util::Error& e) noexcept {
    g_code = e.code;
    g_msg  = e.message;
}

void clear_last_error() noexcept {
    g_code = 0;
    g_msg.clear();
}

std::int32_t last_error_code() noexcept { return g_code; }
const std::string& last_error_message() noexcept { return g_msg; }

} // namespace openads::abi
```

- [ ] **Step 5: Write `src/abi/charset.h`**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace openads::abi {

// Phase 1 placeholder: M1 treats input/output as already-correct byte
// sequences. Real OEM/ANSI/UTF translation lands in M4 alongside the
// `*W` entry-point variants.
std::string to_internal(const std::uint8_t* p, std::size_t n);
void copy_to_caller(std::uint8_t* dst, std::uint16_t* dst_len_inout,
                    const std::string& src) noexcept;

} // namespace openads::abi
```

- [ ] **Step 6: Replace `src/abi/charset.cpp`**

```cpp
#include "abi/charset.h"

#include <algorithm>
#include <cstring>

namespace openads::abi {

std::string to_internal(const std::uint8_t* p, std::size_t n) {
    if (p == nullptr) return {};
    if (n == 0) {
        // ACE convention: NUL-terminated when length is 0.
        n = std::strlen(reinterpret_cast<const char*>(p));
    }
    return std::string(reinterpret_cast<const char*>(p), n);
}

void copy_to_caller(std::uint8_t* dst, std::uint16_t* dst_len_inout,
                    const std::string& src) noexcept {
    if (dst == nullptr || dst_len_inout == nullptr) return;
    std::uint16_t cap = *dst_len_inout;
    std::uint16_t n   = static_cast<std::uint16_t>(
        std::min<std::size_t>(src.size(), cap == 0 ? 0 : cap - 1));
    std::memcpy(dst, src.data(), n);
    if (cap > 0) dst[n] = '\0';
    *dst_len_inout = n;
}

} // namespace openads::abi
```

- [ ] **Step 7: Replace `src/abi/ace_exports.cpp`**

```cpp
#include "openads/ace.h"
#include "openads/error.h"

#include "abi/charset.h"
#include "abi/last_error.h"

#include "engine/table.h"
#include "session/connection.h"
#include "session/handle_registry.h"
#include "drivers/dbf_common.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace {

using openads::engine::Table;
using openads::session::Connection;
using openads::session::Handle;
using openads::session::HandleKind;

struct ProcessState {
    std::mutex                                                    mu;
    openads::session::HandleRegistry                              registry;
    std::unordered_map<Handle, std::unique_ptr<Connection>>       conns;
};

ProcessState& state() {
    static ProcessState s;
    return s;
}

UNSIGNED32 ok() {
    openads::abi::clear_last_error();
    return openads::AE_SUCCESS;
}

UNSIGNED32 fail(const openads::util::Error& e) {
    openads::abi::set_last_error(e);
    return static_cast<UNSIGNED32>(e.code);
}

UNSIGNED32 fail(int code, const char* msg) {
    return fail(openads::util::Error{code, 0, msg ? msg : "", ""});
}

openads::engine::TableType map_type(UNSIGNED16 t) {
    switch (t) {
        case ADS_NTX: return openads::engine::TableType::Ntx;
        case ADS_CDX: return openads::engine::TableType::Cdx;
        case ADS_ADT: return openads::engine::TableType::Adt;
        case ADS_VFP: return openads::engine::TableType::Vfp;
        default:      return openads::engine::TableType::Cdx;
    }
}

UNSIGNED16 map_field_type(openads::drivers::DbfFieldType t) {
    using openads::drivers::DbfFieldType;
    switch (t) {
        case DbfFieldType::Character: return ADS_FIELD_TYPE_CHAR;
        case DbfFieldType::Numeric:
        case DbfFieldType::Float:     return ADS_FIELD_TYPE_NUMERIC;
        case DbfFieldType::Logical:   return ADS_FIELD_TYPE_LOGICAL;
        case DbfFieldType::Date:      return ADS_FIELD_TYPE_DATE;
        case DbfFieldType::DateTime:  return ADS_FIELD_TYPE_DATETIME;
        case DbfFieldType::Memo:      return ADS_FIELD_TYPE_MEMO;
        case DbfFieldType::Integer:   return ADS_FIELD_TYPE_INTEGER;
        case DbfFieldType::Currency:  return ADS_FIELD_TYPE_CURRENCY;
        case DbfFieldType::Double:    return ADS_FIELD_TYPE_DOUBLE;
        case DbfFieldType::Unknown:   return ADS_FIELD_TYPE_UNKNOWN;
    }
    return ADS_FIELD_TYPE_UNKNOWN;
}

const openads::drivers::DbfField*
find_field(Table* tbl, const std::string& name) {
    for (std::uint16_t i = 0; i < tbl->field_count(); ++i) {
        const auto& f = tbl->field_descriptor(i);
        if (f.name == name) return &f;
    }
    return nullptr;
}

} // namespace

extern "C" {

UNSIGNED32 AdsConnect60(UNSIGNED8* pucServer, UNSIGNED16 /*usServerType*/,
                        UNSIGNED8* /*pucUser*/, UNSIGNED8* /*pucPwd*/,
                        UNSIGNED32 /*ulOptions*/, ADSHANDLE* phConnect) {
    if (phConnect == nullptr) return fail(openads::AE_INTERNAL_ERROR,
                                          "phConnect is null");
    auto path = openads::abi::to_internal(pucServer, 0);
    auto opened = Connection::open(path);
    if (!opened) return fail(opened.error());
    auto holder = std::make_unique<Connection>(std::move(opened).value());
    Connection* raw = holder.get();
    auto& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    Handle h = s.registry.register_object(HandleKind::Connection, raw);
    s.conns.emplace(h, std::move(holder));
    *phConnect = h;
    return ok();
}

UNSIGNED32 AdsDisconnect(ADSHANDLE hConnect) {
    auto& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    s.registry.release(hConnect);
    s.conns.erase(hConnect);
    return ok();
}

UNSIGNED32 AdsOpenTable(ADSHANDLE  hConnect,
                        UNSIGNED8* pucName,
                        UNSIGNED8* /*pucAlias*/,
                        UNSIGNED16 usTableType,
                        UNSIGNED16 /*usCharType*/,
                        UNSIGNED16 /*usLockType*/,
                        UNSIGNED16 /*usCheckRights*/,
                        UNSIGNED16 /*usMode*/,
                        ADSHANDLE* phTable) {
    if (phTable == nullptr) return fail(openads::AE_INTERNAL_ERROR,
                                        "phTable is null");
    auto& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    auto* conn = s.registry.lookup<Connection>(hConnect, HandleKind::Connection);
    if (conn == nullptr) return fail(openads::AE_INVALID_CONNECTION_HANDLE,
                                     "unknown connection");
    auto name = openads::abi::to_internal(pucName, 0);
    auto t = conn->open_table(name, map_type(usTableType));
    if (!t) return fail(t.error());
    *phTable = t.value();
    return ok();
}

UNSIGNED32 AdsCloseTable(ADSHANDLE hTable) {
    auto& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    // We don't know which connection owns this handle; iterate.
    for (auto& [h, conn] : s.conns) { (void)h; conn->close_table(hTable); }
    return ok();
}

namespace {
Table* get_table(ADSHANDLE h) {
    auto& s = state();
    return s.registry.lookup<Table>(h, HandleKind::Table);
}
} // namespace

UNSIGNED32 AdsGotoTop(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->goto_top();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsGotoBottom(ADSHANDLE hTable) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->goto_bottom();
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsSkip(ADSHANDLE hTable, SIGNED32 lRows) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    auto r = t->skip(lRows);
    if (!r) return fail(r.error());
    return ok();
}

UNSIGNED32 AdsAtEOF(ADSHANDLE hTable, UNSIGNED16* pbAtEnd) {
    Table* t = get_table(hTable);
    if (!t || pbAtEnd == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    *pbAtEnd = t->eof() ? 1 : 0;
    return ok();
}

UNSIGNED32 AdsAtBOF(ADSHANDLE hTable, UNSIGNED16* pbAtBegin) {
    Table* t = get_table(hTable);
    if (!t || pbAtBegin == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    *pbAtBegin = t->bof() ? 1 : 0;
    return ok();
}

UNSIGNED32 AdsGetNumFields(ADSHANDLE hTable, UNSIGNED16* pusFields) {
    Table* t = get_table(hTable);
    if (!t || pusFields == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    *pusFields = t->field_count();
    return ok();
}

UNSIGNED32 AdsGetFieldName(ADSHANDLE hTable, UNSIGNED16 usFieldNum,
                           UNSIGNED8* pucBuf, UNSIGNED16* pusLen) {
    Table* t = get_table(hTable);
    if (!t) return fail(openads::AE_INTERNAL_ERROR, "unknown table");
    if (usFieldNum == 0 || usFieldNum > t->field_count()) {
        return fail(openads::AE_COLUMN_NOT_FOUND, "field index out of range");
    }
    const auto& f = t->field_descriptor(usFieldNum - 1);
    openads::abi::copy_to_caller(pucBuf, pusLen, f.name);
    return ok();
}

UNSIGNED32 AdsGetFieldType(ADSHANDLE hTable, UNSIGNED8* pucField,
                           UNSIGNED16* pusType) {
    Table* t = get_table(hTable);
    if (!t || pusType == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    auto name = openads::abi::to_internal(pucField, 0);
    const auto* f = find_field(t, name);
    if (f == nullptr) return fail(openads::AE_COLUMN_NOT_FOUND, name.c_str());
    *pusType = map_field_type(f->type);
    return ok();
}

UNSIGNED32 AdsGetFieldLength(ADSHANDLE hTable, UNSIGNED8* pucField,
                             UNSIGNED32* pulLen) {
    Table* t = get_table(hTable);
    if (!t || pulLen == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    auto name = openads::abi::to_internal(pucField, 0);
    const auto* f = find_field(t, name);
    if (f == nullptr) return fail(openads::AE_COLUMN_NOT_FOUND, name.c_str());
    *pulLen = f->length;
    return ok();
}

UNSIGNED32 AdsGetRecordNum(ADSHANDLE hTable, UNSIGNED16 /*bFilterOption*/,
                           UNSIGNED32* pulRecordNum) {
    Table* t = get_table(hTable);
    if (!t || pulRecordNum == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    *pulRecordNum = t->recno();
    return ok();
}

UNSIGNED32 AdsGetRecordCount(ADSHANDLE hTable, UNSIGNED16 /*bFilterOption*/,
                             UNSIGNED32* pulRecordCount) {
    Table* t = get_table(hTable);
    if (!t || pulRecordCount == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    *pulRecordCount = t->record_count();
    return ok();
}

UNSIGNED32 AdsGetField(ADSHANDLE hTable, UNSIGNED8* pucField,
                       UNSIGNED8* pucBuf, UNSIGNED32* pulLen,
                       UNSIGNED16 /*usOption*/) {
    Table* t = get_table(hTable);
    if (!t || pulLen == nullptr) return fail(openads::AE_INTERNAL_ERROR, "");
    auto name = openads::abi::to_internal(pucField, 0);
    std::uint16_t idx = 0;
    bool found = false;
    for (std::uint16_t i = 0; i < t->field_count(); ++i) {
        if (t->field_descriptor(i).name == name) { idx = i; found = true; break; }
    }
    if (!found) return fail(openads::AE_COLUMN_NOT_FOUND, name.c_str());
    auto v = t->read_field(idx);
    if (!v) return fail(v.error());
    UNSIGNED16 cap = static_cast<UNSIGNED16>(
        *pulLen > 0xFFFFu ? 0xFFFFu : *pulLen);
    UNSIGNED16 cap_inout = cap;
    openads::abi::copy_to_caller(pucBuf, &cap_inout, v.value().as_string);
    *pulLen = cap_inout;
    return ok();
}

UNSIGNED32 AdsGetLastError(UNSIGNED32* pulCode, UNSIGNED8* pucBuf,
                           UNSIGNED16* pusBufLen) {
    if (pulCode != nullptr) *pulCode = static_cast<UNSIGNED32>(
        openads::abi::last_error_code());
    if (pucBuf != nullptr && pusBufLen != nullptr) {
        openads::abi::copy_to_caller(pucBuf, pusBufLen,
                                     openads::abi::last_error_message());
    }
    return openads::AE_SUCCESS;
}

UNSIGNED32 AdsGetVersion(UNSIGNED32* pulMajor, UNSIGNED32* pulMinor,
                         UNSIGNED32* pulLetter, UNSIGNED32* pulDesc) {
    if (pulMajor != nullptr)  *pulMajor  = 0;
    if (pulMinor != nullptr)  *pulMinor  = 0;
    if (pulLetter != nullptr) *pulLetter = 'a';
    if (pulDesc != nullptr)   *pulDesc   = 1;
    return openads::AE_SUCCESS;
}

} // extern "C"
```

- [ ] **Step 8: Build to verify the ABI layer compiles**

Run:

```
cmake --build build/default --config Release
```

Expected: success.

- [ ] **Step 9: Commit**

```
git add include/openads/ace.h src/abi/ace_types.h src/abi/last_error.h src/abi/last_error.cpp src/abi/charset.h src/abi/charset.cpp src/abi/ace_exports.cpp
git commit -m "feat(abi): ACE C ABI thunks for the M1 read-only entry points"
```

---

## Task 10: ABI smoke test

**Files:**
- Modify: `c:/OpenADS/tests/unit/abi_smoke_test.cpp`

- [ ] **Step 1: Write the end-to-end test**

Replace `c:/OpenADS/tests/unit/abi_smoke_test.cpp`:

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

fs::path make_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    fs::remove(p);

    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 2; // 2 records
    hdr[8]  = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 4;       hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());

    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C';
    fd[16] = 4;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);

    auto push_rec = [&](const char* name) {
        file.push_back(' ');
        for (int i = 0; i < 4; ++i) {
            file.push_back(static_cast<std::uint8_t>(
                i < static_cast<int>(std::strlen(name)) ? name[i] : ' '));
        }
    };
    push_rec("AB");
    push_rec("CDEF");
    file.push_back(0x1A);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("ABI smoke: open dir, open table, walk records, read field") {
    const auto dir = fs::temp_directory_path() / "openads_m1_abi";
    fs::remove_all(dir);
    make_dbf(dir, "data.dbf");

    ADSHANDLE hConn = 0;
    UNSIGNED8 server_buf[256];
    std::memcpy(server_buf, dir.string().c_str(), dir.string().size() + 1);
    REQUIRE(AdsConnect60(server_buf, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name_buf[64] = "data.dbf";
    REQUIRE(AdsOpenTable(hConn, name_buf, nullptr, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    UNSIGNED16 fields = 0;
    REQUIRE(AdsGetNumFields(hTable, &fields) == 0);
    CHECK(fields == 1);

    UNSIGNED32 rec_count = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &rec_count) == 0);
    CHECK(rec_count == 2);

    REQUIRE(AdsGotoTop(hTable) == 0);

    auto read_first_field = [&](std::string& out) {
        UNSIGNED8 fname[64] = "TAG";
        UNSIGNED8 buf[64]   = {0};
        UNSIGNED32 cap = sizeof(buf);
        UNSIGNED32 r = AdsGetField(hTable, fname, buf, &cap, 0);
        REQUIRE(r == 0);
        out.assign(reinterpret_cast<const char*>(buf), cap);
    };

    std::string row1;
    read_first_field(row1);
    CHECK(row1 == "AB");

    REQUIRE(AdsSkip(hTable, 1) == 0);
    std::string row2;
    read_first_field(row2);
    CHECK(row2 == "CDEF");

    REQUIRE(AdsSkip(hTable, 1) == 0);
    UNSIGNED16 at_eof = 0;
    REQUIRE(AdsAtEOF(hTable, &at_eof) == 0);
    CHECK(at_eof == 1);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);

    fs::remove_all(dir);
}
```

- [ ] **Step 2: Run tests to verify the new case passes**

Run:

```
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: 1 new test case passes; the suite total grows by 1.

- [ ] **Step 3: Commit**

```
git add tests/unit/abi_smoke_test.cpp
git commit -m "test(abi): end-to-end smoke through AdsConnect60..AdsGetField"
```

---

## Task 11: Update README and tag M1

**Files:**
- Modify: `c:/OpenADS/README.md`

- [ ] **Step 1: Mark M1 done in the milestone table**

Inside the `## Next steps` table, replace the M1 row with:

```markdown
| **M1 — DBF read (CDX)** | [`2026-05-03-openads-m1-dbf-read.md`](docs/superpowers/plans/2026-05-03-openads-m1-dbf-read.md) | **Done.** Read-only DBF (`ADS_CDX` typed) via `AdsConnect60` / `AdsOpenTable` / `AdsGotoTop` / `AdsSkip` / `AdsGetField` and friends. No memo (M4), no index (M3), no write (M2). |
```

- [ ] **Step 2: Commit, tag, push**

Run from `c:/OpenADS`:

```
git add README.md
git commit -m "docs: mark M1 milestone done"
git tag m1-done
git push origin main --tags
```

---

## Done

At the end of M1:

- The 18 ACE C ABI entry points listed in this plan all return real data over a CDX-typed DBF.
- A self-contained doctest (`abi_smoke_test.cpp`) walks a fixture DBF end-to-end through the L1 ABI and verifies field values byte-for-byte.
- ~17 new test cases pass on top of the 27 from M0.
- The repository is ready for M2 (DBF write + LockMgr + NTX driver) to land on top of `dbf_common`, `Table`, `Connection`, and the L1 thunks established here.
