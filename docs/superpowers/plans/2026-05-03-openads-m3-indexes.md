# OpenADS — M3 Index Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring up index-driven navigation: open an `.ntx` (Clipper) and `.cdx` (FoxPro multi-tag) index file, walk records in index order, seek by key, set top/bottom scope, and create a new NTX from the data file. End-to-end test: an L1 sequence opens a DBF + NTX pair, sets the order, seeks for a value, walks two records in index order, sets a scope, and reads back the bounded set.

**Architecture:** New L4 abstractions: `IIndex` driver trait with `read_root_keys` / `seek_key` / `next_key` / `prev_key` / `insert_key` / `delete_key` / `create`; concrete drivers `NtxIndex` (full read + write + create) and `CdxIndex` (multi-tag read-only — write lands in a follow-up). `Table` grows an `Order` slot, `Scope` (top + bottom keys), and key-driven `goto_top` / `goto_bottom` / `skip` semantics that delegate to the active order when one is set. The ADT `.adi` driver and the AOF (Advantage Optimized Filter) layer are stubbed to return `AE_FUNCTION_NOT_AVAILABLE` until M4 / a future milestone. `Pack` / `Zap` likewise stub for now: a faithful implementation requires the memo store landing in M4.

**Tech Stack:** Same as previous milestones (C++17, CMake, doctest). No new third-party deps.

---

## Scope at a glance

| Surface | M3 status |
|---------|-----------|
| NTX read | full |
| NTX write (insert / delete / split) | full |
| NTX create from data | full |
| CDX read (multi-tag, leaf walk, seek) | full |
| CDX write (insert / erase / split / tag-dir update) | full |
| ADI read / write | **stub** (`AE_FUNCTION_NOT_AVAILABLE`) — M4 with the ADT driver |
| AOF (Advantage Optimized Filter) | **stub** — `ADS_OPTIMIZED_NONE` always |
| Pack / Zap | **stub** until memo store lands in M4 |
| `AdsSeek` / `AdsSeekLast` | full |
| `AdsSetScope` / `AdsClearScope` / `AdsGetScope` | full |
| `AdsSetOrder` / `AdsCreateIndex` (NTX) | full |

---

## File structure for this milestone

Touched in M3:

```
OpenADS/
├── src/
│   ├── drivers/
│   │   ├── index_trait.h           # NEW — IIndex abstract interface
│   │   ├── ntx/
│   │   │   ├── ntx_index.h         # NEW
│   │   │   └── ntx_index.cpp       # NEW
│   │   └── cdx/
│   │       ├── cdx_index.h         # NEW
│   │       └── cdx_index.cpp       # NEW
│   ├── engine/
│   │   ├── order.h                 # NEW — index order + scope
│   │   ├── order.cpp               # NEW
│   │   ├── table.{h,cpp}           # extended with order, scope, key seek
│   └── abi/
│       └── ace_exports.cpp         # 15 new thunks + 5 stubs
├── include/openads/ace.h           # +declarations + index option enums
└── tests/unit/
    ├── ntx_index_test.cpp          # NEW
    ├── cdx_index_test.cpp          # NEW
    ├── engine_order_test.cpp       # NEW
    ├── engine_scope_test.cpp       # NEW
    └── abi_index_smoke_test.cpp    # NEW
```

Boundaries:

- `index_trait.h` keeps `IIndex` independent of `IDriver`; an order is a separate concept from a table.
- All format knowledge stays in `drivers/<format>/` — engine code never reads index page bytes.
- `Order` owns the active index handle and the scope; `Table` delegates navigation to `Order` when one is set.

---

## Task 1: `IIndex` trait + register new modules in build

**Files:**
- Create: `c:/OpenADS/src/drivers/index_trait.h`
- Modify: `c:/OpenADS/src/CMakeLists.txt`
- Modify: `c:/OpenADS/tests/CMakeLists.txt`
- Create stubs: `c:/OpenADS/src/drivers/ntx/ntx_index.{h,cpp}`, `c:/OpenADS/src/drivers/cdx/cdx_index.{h,cpp}`, `c:/OpenADS/src/engine/order.{h,cpp}`, and 5 test files.

- [ ] **Step 1: Write `index_trait.h`**

```cpp
#pragma once

#include "drivers/dbf_common.h"
#include "platform/file.h"
#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openads::drivers {

enum class IndexOpenMode { ReadOnly, Shared, Exclusive };

// SeekResult: which way the search landed.
enum class SeekHit { Exact, AfterKey, BeforeBegin, AfterEnd };

struct SeekOutcome {
    SeekHit       hit          = SeekHit::AfterEnd;
    std::uint32_t recno        = 0;     // recno at the cursor position
    bool          positioned   = false;
};

class IIndex {
public:
    virtual ~IIndex() = default;

    virtual util::Result<void>
        open(const std::string& path, IndexOpenMode mode) = 0;

    virtual std::string name()       const = 0;
    virtual std::string expression() const = 0;
    virtual bool        descending() const = 0;
    virtual bool        unique()     const = 0;

    // Cursor positioning over the index.
    virtual util::Result<SeekOutcome> seek_first()   = 0;
    virtual util::Result<SeekOutcome> seek_last()    = 0;
    virtual util::Result<SeekOutcome>
        seek_key(const std::string& key, bool soft) = 0;
    virtual util::Result<SeekOutcome> next()         = 0;
    virtual util::Result<SeekOutcome> prev()         = 0;

    // Returns the key at the current cursor position, encoded as the
    // raw bytes the index stores. Engine code does not interpret it.
    virtual std::string current_key() const = 0;

    // Mutating ops; CdxIndex's write surface stubs until follow-up.
    virtual util::Result<void> insert(std::uint32_t recno,
                                      const std::string& key) = 0;
    virtual util::Result<void> erase (std::uint32_t recno,
                                      const std::string& key) = 0;
    virtual util::Result<void> flush() = 0;
};

} // namespace openads::drivers
```

- [ ] **Step 2: Write stubs for `ntx_index.{h,cpp}`, `cdx_index.{h,cpp}`, `engine/order.{h,cpp}` so the rest of the plan can be filled in incrementally**

Each `.h` carries `#pragma once` and a forward-declared empty class deriving from `IIndex` with `=default`-able dtor; each `.cpp` contains the placeholder line `// placeholder, real content lands in a later task`.

For now it is enough to declare the classes so the headers compile when included:

`c:/OpenADS/src/drivers/ntx/ntx_index.h`:

```cpp
#pragma once

#include "drivers/index_trait.h"

namespace openads::drivers::ntx {

class NtxIndex final : public IIndex {
public:
    util::Result<void> open(const std::string&, IndexOpenMode) override
    { return util::Error{5004, 0, "NtxIndex not yet implemented", ""}; }

    std::string name()       const override { return {}; }
    std::string expression() const override { return {}; }
    bool        descending() const override { return false; }
    bool        unique()     const override { return false; }

    util::Result<SeekOutcome> seek_first() override { return SeekOutcome{}; }
    util::Result<SeekOutcome> seek_last()  override { return SeekOutcome{}; }
    util::Result<SeekOutcome> seek_key(const std::string&, bool) override { return SeekOutcome{}; }
    util::Result<SeekOutcome> next()       override { return SeekOutcome{}; }
    util::Result<SeekOutcome> prev()       override { return SeekOutcome{}; }
    std::string current_key() const override { return {}; }

    util::Result<void> insert(std::uint32_t, const std::string&) override
    { return util::Error{5004, 0, "NtxIndex insert not yet implemented", ""}; }
    util::Result<void> erase (std::uint32_t, const std::string&) override
    { return util::Error{5004, 0, "NtxIndex erase not yet implemented", ""}; }
    util::Result<void> flush() override { return {}; }
};

} // namespace openads::drivers::ntx
```

`c:/OpenADS/src/drivers/cdx/cdx_index.h`:

```cpp
#pragma once

#include "drivers/index_trait.h"

namespace openads::drivers::cdx {

class CdxIndex final : public IIndex {
public:
    util::Result<void> open(const std::string&, IndexOpenMode) override
    { return util::Error{5004, 0, "CdxIndex not yet implemented", ""}; }

    std::string name()       const override { return {}; }
    std::string expression() const override { return {}; }
    bool        descending() const override { return false; }
    bool        unique()     const override { return false; }

    util::Result<SeekOutcome> seek_first() override { return SeekOutcome{}; }
    util::Result<SeekOutcome> seek_last()  override { return SeekOutcome{}; }
    util::Result<SeekOutcome> seek_key(const std::string&, bool) override { return SeekOutcome{}; }
    util::Result<SeekOutcome> next()       override { return SeekOutcome{}; }
    util::Result<SeekOutcome> prev()       override { return SeekOutcome{}; }
    std::string current_key() const override { return {}; }

    util::Result<void> insert(std::uint32_t, const std::string&) override
    { return util::Error{5004, 0, "CdxIndex insert not yet implemented", ""}; }
    util::Result<void> erase (std::uint32_t, const std::string&) override
    { return util::Error{5004, 0, "CdxIndex erase not yet implemented", ""}; }
    util::Result<void> flush() override { return {}; }
};

} // namespace openads::drivers::cdx
```

Both `.cpp` files: `// placeholder, real content lands in a later task`.

`c:/OpenADS/src/engine/order.h`:

```cpp
#pragma once

#include "drivers/index_trait.h"

#include <memory>
#include <optional>
#include <string>

namespace openads::engine {

struct Scope {
    std::optional<std::string> top;
    std::optional<std::string> bottom;
};

class Order {
public:
    Order() = default;
    explicit Order(std::unique_ptr<drivers::IIndex> idx) noexcept
        : index_(std::move(idx)) {}
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;

    drivers::IIndex* index() noexcept { return index_.get(); }
    const drivers::IIndex* index() const noexcept { return index_.get(); }

    Scope&       scope()       noexcept { return scope_; }
    const Scope& scope() const noexcept { return scope_; }

private:
    std::unique_ptr<drivers::IIndex> index_;
    Scope                            scope_;
};

} // namespace openads::engine
```

`c:/OpenADS/src/engine/order.cpp`: `// placeholder, real content lands in a later task`.

- [ ] **Step 3: Update `src/CMakeLists.txt`** — add `engine/order.cpp`, `drivers/ntx/ntx_index.cpp`, `drivers/cdx/cdx_index.cpp` to the source list.

- [ ] **Step 4: Update `tests/CMakeLists.txt`** — register five new test stubs:

```
unit/ntx_index_test.cpp
unit/cdx_index_test.cpp
unit/engine_order_test.cpp
unit/engine_scope_test.cpp
unit/abi_index_smoke_test.cpp
```

Each new test file gets the `#include "doctest.h"` placeholder.

- [ ] **Step 5: Configure + build + run tests**

```
cmake --preset default
cmake --build build/default --config Release
build/default/tests/Release/openads_unit_tests.exe
```

Expected: existing 65 cases still pass; suite total unchanged.

- [ ] **Step 6: Commit**

```
git add src/drivers/index_trait.h src/drivers/ntx/ntx_index.{h,cpp} src/drivers/cdx/cdx_index.{h,cpp} src/engine/order.{h,cpp} src/CMakeLists.txt tests/CMakeLists.txt tests/unit/ntx_index_test.cpp tests/unit/cdx_index_test.cpp tests/unit/engine_order_test.cpp tests/unit/engine_scope_test.cpp tests/unit/abi_index_smoke_test.cpp
git commit -m "build: M3 module skeleton (IIndex, NtxIndex, CdxIndex, Order)"
```

---

## Task 2: NTX format read

**Files:**
- Modify: `c:/OpenADS/src/drivers/ntx/ntx_index.{h,cpp}`
- Modify: `c:/OpenADS/tests/unit/ntx_index_test.cpp`

NTX file layout (Clipper, 1024-byte pages):

```
Page 0 (header, 1024 bytes):
  bytes 0-1   : signature 0x0006 (LE)
  bytes 2-3   : indexing version (LE)
  bytes 4-7   : root page offset (LE)
  bytes 8-11  : next available page offset (LE)
  bytes 12-13 : key size (LE)
  bytes 14-15 : key + recno entry size (= key_size + 8) (LE)
  bytes 16-17 : max keys per page (LE)
  bytes 18-19 : half page (max_keys / 2)
  bytes 20-23 : flags (unique, descending, etc.)
  bytes 24-279: key expression (NUL-terminated, 256 bytes)
  bytes 280-...: padding

Each non-header page:
  bytes 0-1   : key count
  for each key (offset table at bytes 2 .. 2 + 2*max_keys):
    each entry contains a 16-bit offset to the actual key block
  key blocks contain:
    4 bytes : left child page (0 if leaf)
    4 bytes : recno
    N bytes : key data (padded with spaces if shorter)
```

This task implements only **read** of an existing NTX. Writes land in Task 3.

- [ ] **Step 1: Write the failing tests**

Replace `tests/unit/ntx_index_test.cpp`:

```cpp
#include "doctest.h"
#include "drivers/ntx/ntx_index.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using openads::drivers::IndexOpenMode;
using openads::drivers::ntx::NtxIndex;

namespace {

// Build a 2-page NTX: header + one leaf with two keys "AAAA" → recno 1
// and "BBBB" → recno 2 (key size 4, max 2 keys for the test fixture).
fs::path make_simple_ntx(const char* tag) {
    auto p = fs::temp_directory_path() /
             (std::string("openads_m3_ntx_") + tag);
    fs::remove(p);
    std::vector<std::uint8_t> file(2048, 0);

    // ---- Page 0: header ----
    auto put_u16 = [&](std::size_t off, std::uint16_t v) {
        file[off]     = static_cast<std::uint8_t>( v       & 0xFF);
        file[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    };
    auto put_u32 = [&](std::size_t off, std::uint32_t v) {
        file[off]     = static_cast<std::uint8_t>( v        & 0xFF);
        file[off + 1] = static_cast<std::uint8_t>((v >>  8) & 0xFF);
        file[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
        file[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    };
    put_u16(0,  0x0006);  // signature
    put_u16(2,  0x0001);  // version
    put_u32(4,  1024);    // root page offset
    put_u32(8,  2048);    // next available page
    put_u16(12, 4);       // key size
    put_u16(14, 4 + 8);   // entry size = key + 8
    put_u16(16, 2);       // max keys per page
    put_u16(18, 1);       // half-page
    put_u32(20, 0);       // flags

    const char* expr = "TAG";
    std::memcpy(file.data() + 24, expr, std::strlen(expr));

    // ---- Page 1 (offset 1024): leaf with 2 keys ----
    std::size_t leaf = 1024;
    put_u16(leaf, 2);                              // 2 keys

    // entry[0] -> offset to key block within page
    std::size_t entry0_off = 2 + 0 * 2;
    std::size_t entry1_off = 2 + 1 * 2;
    std::size_t blk0_off   = 2 + 2 * 2 + 2;        // start of key blocks
    std::size_t blk1_off   = blk0_off + (4 + 4 + 4);
    put_u16(leaf + entry0_off, static_cast<std::uint16_t>(blk0_off));
    put_u16(leaf + entry1_off, static_cast<std::uint16_t>(blk1_off));

    auto put_key = [&](std::size_t off, std::uint32_t recno, const char* k) {
        put_u32(leaf + off, 0);                    // left child = 0 (leaf)
        put_u32(leaf + off + 4, recno);
        std::memcpy(file.data() + leaf + off + 8, k, 4);
    };
    put_key(blk0_off, 1, "AAAA");
    put_key(blk1_off, 2, "BBBB");

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("NtxIndex opens and exposes the key expression") {
    auto p = make_simple_ntx("open");
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::ReadOnly).has_value());
        CHECK(ix.expression() == "TAG");
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex seek_first / next walks keys in order") {
    auto p = make_simple_ntx("walk");
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::ReadOnly).has_value());

        auto a = ix.seek_first();
        REQUIRE(a.has_value());
        CHECK(a.value().positioned);
        CHECK(a.value().recno == 1);
        CHECK(ix.current_key() == "AAAA");

        auto b = ix.next();
        REQUIRE(b.has_value());
        CHECK(b.value().positioned);
        CHECK(b.value().recno == 2);
        CHECK(ix.current_key() == "BBBB");

        auto c = ix.next();
        REQUIRE(c.has_value());
        CHECK_FALSE(c.value().positioned);
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex seek_key locates an exact key") {
    auto p = make_simple_ntx("seek");
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::ReadOnly).has_value());
        auto r = ix.seek_key("BBBB", false);
        REQUIRE(r.has_value());
        CHECK(r.value().hit == openads::drivers::SeekHit::Exact);
        CHECK(r.value().recno == 2);
    }
    fs::remove(p);
}
```

- [ ] **Step 2: Write `ntx_index.h`** with full read interface (see test expectations above; member fields include `platform::File`, header values, current page + key index, key buffer).

- [ ] **Step 3: Implement `ntx_index.cpp`** — header parse, leaf-only walk with descent helper for soft seek, current-key buffer.

(Keep this implementation focused: in M3 the test fixtures are leaf-only, and the implementation handles a single root page that is also a leaf. Multi-level descent lands in Task 3 alongside writes.)

- [ ] **Step 4: Build + run tests** — expected: 3 new cases pass.

- [ ] **Step 5: Commit**

```
git commit -m "feat(drivers/ntx): NTX index read (header + leaf walk + exact seek)"
```

---

## Task 3: NTX write — insert / erase / flush / split

Build on Task 2's reader. Add:

- A page allocator that bumps `next_avail` and zeroes the new page.
- `insert(recno, key)`: locate the leaf, splice into the offset + key-block table, split when `keys_count == max_keys` by promoting a median key into the parent page (creating a parent if the leaf is the root).
- `erase(recno, key)`: remove the entry; do not bother rebalancing in M3 (acceptable: empty pages stay reachable but unused).
- `flush()`: dirty-page set, write each, then `file().sync()`.
- A `create(...)` static factory used by `AdsCreateIndex`.

Tests:

```cpp
TEST_CASE("NtxIndex insert appends a key and seek finds it") { ... }
TEST_CASE("NtxIndex insert triggers a leaf split") { ... }
TEST_CASE("NtxIndex erase removes a key") { ... }
TEST_CASE("NtxIndex create builds an index over an in-memory record stream") { ... }
```

(Each test follows the bite-sized red→green→commit pattern. Bodies here keep the plan readable: build a fixture, drive `insert`, reopen and walk, assert the order.)

- [ ] **Step 1: Failing tests** — write all four into `tests/unit/ntx_index_test.cpp`.

- [ ] **Step 2: Implement** — extend `NtxIndex` with the page allocator + split logic. Document the split contract: `insert` returns success when keys_count < max_keys; otherwise allocates a new sibling, copies the upper half, promotes the smallest of the upper half into the parent, recurses.

- [ ] **Step 3: Run tests** — 4 new cases pass.

- [ ] **Step 4: Commit**

```
git commit -m "feat(drivers/ntx): NTX index insert / erase / split + create"
```

---

## Task 4: CDX format read

CDX is a multi-tag, FoxPro-format compact index with 512-byte pages and the following high-level structure:

```
Header page (page 0) at offset 0:
  bytes 0-3   : root page offset (uint32 LE)
  bytes 4-7   : free list head
  bytes 8-11  : version
  bytes 12-13 : key length
  byte 14     : index options (flags)
  byte 15     : signature 0x01
  bytes 16-23 : reserved
  bytes 24-31 : "for" expression length + "for" expression head
  ...
  Tag list lives in a *root tag* page that maps tag name → root page of tag.
```

For M3 we treat the whole CDX as a flat directory of named tags, each with its own root page; we read leaf pages for sequential walk and exact seek. Compact-index page decoding (delta-encoded key bytes, run-length headers) is implemented in this task.

- [ ] **Step 1: Failing tests** — write `tests/unit/cdx_index_test.cpp`:

```cpp
TEST_CASE("CdxIndex enumerates tags") { ... }
TEST_CASE("CdxIndex seek_first / next walks one tag in order") { ... }
TEST_CASE("CdxIndex seek_key finds an exact hit") { ... }
```

Build a hand-crafted CDX with two tags, two keys each, using a helper that emits the FoxPro compact-page layout.

- [ ] **Step 2: Write `cdx_index.h`** — same `IIndex` surface plus a `tags()` accessor that returns names. The active tag is selected at `open` time via a path-and-tag-name pair, e.g. `open("foo.cdx", IndexOpenMode::ReadOnly, "byname")`.

- [ ] **Step 3: Implement `cdx_index.cpp`** — header parse, tag enumeration, leaf walk decoding compact entries, soft and exact seek.

- [ ] **Step 4: Run tests** — 3 new cases pass.

- [ ] **Step 5: Commit**

```
git commit -m "feat(drivers/cdx): CDX read — tag enumeration, leaf walk, seek"
```

---

## Task 5: CDX write — insert / erase / split / tag-dir update

Extend `CdxIndex` with the FoxPro compact-format write path:

- **Page allocator**: bump `next_avail` (free list head when non-empty), zero the new page, write its initial header bytes (`page_type`, `key_count = 0`, `prev/next` siblings, `free_offset`).
- **Compact entry encoding**: each leaf entry is stored as a packed triple `(recno, duplicate_count, trail_count)` where `duplicate_count` is the number of leading bytes shared with the previous entry's key, and `trail_count` is the number of trailing space bytes elided. Suffix bytes follow. Bit packing is parameterised by the header's `recno_mask`, `dup_mask`, `trail_mask`. Provide an `encode_leaf_entry` / `decode_leaf_entry` pair shared between read and write.
- **Internal nodes**: distinct format (4-byte `recno`, `key_len` bytes of full key, 4-byte `child_page`). No compression.
- **`insert(recno, key)`**:
  1. Descend from the active tag's root to the target leaf, collecting the descent path.
  2. Decompress the leaf into a temp buffer of plain `(recno, full_key)` tuples.
  3. Insert in sorted order (or fail with `5044 AE_DUPLICATE_KEY` for unique tags).
  4. Re-encode. If the encoded buffer fits the page, rewrite it in place.
  5. Otherwise split: allocate sibling, partition entries roughly in half, encode both pages, promote a separator key (the first key of the new sibling, in plain form) into the parent. Walk the descent path upward; if the root splits, allocate a new internal root and update the tag directory entry to point at it.
- **`erase(recno, key)`**: decompress, drop the matching entry (must match recno), re-encode. No active rebalancing — short pages are tolerated.
- **`flush`**: writes dirty pages, then `file().sync()`.
- **Free list**: deleted pages are linked into the per-CDX free list referenced by the header.

Tests in `cdx_index_test.cpp`:

```cpp
TEST_CASE("CdxIndex insert appends a key into an existing tag and seek_key finds it") { ... }
TEST_CASE("CdxIndex insert into a unique tag rejects duplicates with AE_DUPLICATE_KEY") { ... }
TEST_CASE("CdxIndex insert triggers a leaf split and walks across the new sibling") { ... }
TEST_CASE("CdxIndex split that bubbles to the root rewrites the tag directory entry") { ... }
TEST_CASE("CdxIndex erase removes a key and walk skips it") { ... }
```

- [ ] **Step 1: Failing tests** — add the five cases above to `tests/unit/cdx_index_test.cpp`.

- [ ] **Step 2: Implement** the write surface in `cdx_index.cpp`. Keep the encoder a free function so it can be unit-tested in isolation; round-trip every fixture through `encode_leaf_entry` ↔ `decode_leaf_entry`.

- [ ] **Step 3: Run tests** — 5 new cases pass.

- [ ] **Step 4: Commit**

```
git commit -m "feat(drivers/cdx): CDX write — compact leaf encode + split + tag-dir update"
```

---

## Task 6: `Order` plumbing in `Table` + scope semantics

Extend `engine::Table`:

- `set_order(std::unique_ptr<IIndex>)` installs an `Order`; `clear_order()` removes it.
- `goto_top` / `goto_bottom` / `skip` / `seek_key` delegate to the active order when one is set; otherwise keep the M2 sequential semantics.
- `set_scope(top, bottom)` writes into the active `Scope`. EOF/BOF derive from scope edges plus index walk.

Tests in `engine_order_test.cpp` and `engine_scope_test.cpp`:

```cpp
TEST_CASE("Table walks records in the active NTX order") { ... }
TEST_CASE("Table::seek_key positions the cursor at the matching record") { ... }
TEST_CASE("Table::set_scope clamps top/bottom navigation") { ... }
TEST_CASE("Table::clear_scope returns to full-table walk") { ... }
```

- [ ] **Step 1: Failing tests** for both files.
- [ ] **Step 2: Implement** — `Order` ownership in `Table`, scope-aware `goto_top` / `goto_bottom` / `skip` (which call the index's `seek_first` / `seek_last` and walk via `next` / `prev`), `seek_key`.
- [ ] **Step 3: Run tests** — 4 new cases pass.
- [ ] **Step 4: Commit**

```
git commit -m "feat(engine): Order + Scope on Table, index-driven navigation"
```

---

## Task 7: ABI — index entry points and stubs

Add to `include/openads/ace.h`:

```c
UNSIGNED32 AdsOpenIndex     (ADSHANDLE hTable, UNSIGNED8* pucName,
                              ADSHANDLE* phIndex);
UNSIGNED32 AdsCloseIndex    (ADSHANDLE hIndex);
UNSIGNED32 AdsCloseAllIndexes(ADSHANDLE hTable);
UNSIGNED32 AdsCreateIndex   (ADSHANDLE hTable, UNSIGNED8* pucFile,
                              UNSIGNED8* pucTag, UNSIGNED8* pucExpr,
                              UNSIGNED8* pucCondition, UNSIGNED32 ulOptions,
                              UNSIGNED16 usKeyType, ADSHANDLE* phIndex);
UNSIGNED32 AdsDeleteIndex   (ADSHANDLE hIndex);
UNSIGNED32 AdsGetNumIndexes (ADSHANDLE hTable, UNSIGNED16* pusCount);
UNSIGNED32 AdsGetIndexHandle(ADSHANDLE hTable, UNSIGNED8* pucName,
                              ADSHANDLE* phIndex);
UNSIGNED32 AdsGetIndexHandleByOrder(ADSHANDLE hTable, UNSIGNED16 usOrder,
                                    ADSHANDLE* phIndex);
UNSIGNED32 AdsGetIndexExpr  (ADSHANDLE hIndex, UNSIGNED8* pucBuf,
                              UNSIGNED16* pusBufLen);
UNSIGNED32 AdsGetIndexName  (ADSHANDLE hIndex, UNSIGNED8* pucBuf,
                              UNSIGNED16* pusBufLen);
UNSIGNED32 AdsSetIndexDirection(ADSHANDLE hIndex, UNSIGNED16 usDir);

UNSIGNED32 AdsSeek          (ADSHANDLE hIndex, UNSIGNED8* pucKey,
                              UNSIGNED16 usOption, UNSIGNED16* pbFound);
UNSIGNED32 AdsSeekLast      (ADSHANDLE hIndex, UNSIGNED8* pucKey,
                              UNSIGNED16* pbFound);

UNSIGNED32 AdsSetScope      (ADSHANDLE hIndex, UNSIGNED16 usScope,
                              UNSIGNED8* pucKey);
UNSIGNED32 AdsClearScope    (ADSHANDLE hIndex, UNSIGNED16 usScope);
UNSIGNED32 AdsGetScope      (ADSHANDLE hIndex, UNSIGNED16 usScope,
                              UNSIGNED8* pucBuf, UNSIGNED16* pusLen);

UNSIGNED32 AdsPackTable     (ADSHANDLE hTable);   // stub
UNSIGNED32 AdsZapTable      (ADSHANDLE hTable);   // stub
UNSIGNED32 AdsSetAOF        (ADSHANDLE hTable,
                              UNSIGNED8* pucCondition,
                              UNSIGNED16 usResolve);  // stub
UNSIGNED32 AdsGetAOFOptLevel(ADSHANDLE hTable, UNSIGNED16* pusLevel,
                              UNSIGNED8* pucBuf, UNSIGNED16* pusLen); // stub
UNSIGNED32 AdsClearAOF      (ADSHANDLE hTable);   // stub

#define ADS_TOP    0
#define ADS_BOTTOM 1
#define ADS_OPTIMIZED_NONE 3
```

Implement the live thunks in `ace_exports.cpp` against `Table::set_order`, `Table::seek_key`, `Order::scope`, etc. Stubs return `AE_FUNCTION_NOT_AVAILABLE` (`5004`).

- [ ] **Step 1: Append decls + impls.**
- [ ] **Step 2: Build + run smoke** (existing tests still pass).
- [ ] **Step 3: Commit**

```
git commit -m "feat(abi): M3 index / scope / seek entry points + AOF/Pack/Zap stubs"
```

---

## Task 8: ABI integration smoke

`tests/unit/abi_index_smoke_test.cpp` — generates a DBF, calls `AdsCreateIndex` (NTX), `AdsSeek`, `AdsSetOrder`, walks records in index order, applies `AdsSetScope`, verifies the bounded walk, then closes everything.

- [ ] **Step 1: Write the test**.
- [ ] **Step 2: Build + run** — 1 new case passes.
- [ ] **Step 3: Commit**

```
git commit -m "test(abi): end-to-end index smoke (create / seek / scope)"
```

---

## Task 9: README + tag m3-done

- [ ] Mark M3 row done with link to this plan.
- [ ] Commit, tag `m3-done`, push.

---

## Done

At the end of M3:

- NTX read+write+create works end-to-end through the L1 ABI.
- CDX read+seek works for hand-crafted multi-tag fixtures (write deferred).
- `AdsSeek` / `AdsSeekLast` / `AdsSetScope` / `AdsClearScope` operate over the active order.
- `AdsPackTable` / `AdsZapTable` / `AdsSetAOF` / `AdsClearAOF` exist as thunks returning `AE_FUNCTION_NOT_AVAILABLE` until the memo + AOF work in M4.
- ~15 new ACE entry points are wired; the suite grows by ~16 unit tests.
