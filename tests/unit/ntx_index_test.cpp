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
using openads::drivers::SeekHit;
using openads::drivers::ntx::NtxIndex;

namespace {

// Build a 2-page NTX: header + one leaf with two keys.
fs::path make_simple_ntx(const char* tag) {
    auto p = fs::temp_directory_path() /
             (std::string("openads_m3_ntx_") + tag);
    fs::remove(p);
    std::vector<std::uint8_t> file(2048, 0);

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

    // Header (NTXHEADER per Harbour hbrddntx.h):
    //   0  type[2]
    //   2  version[2]
    //   4  root[4]
    //   8  next_page[4]
    //  12  item_size[2]
    //  14  key_size[2]
    //  16  key_dec[2]
    //  18  max_item[2]
    //  20  half_page[2]
    //  22  key_expr[256]
    // 278  unique[1]
    // 279  unknown1[1]
    // 280  descend[1]
    // 281  unknown2[1]
    // 282  for_expr[256]
    // 538  tag_name[12]
    // 550  custom[1]
    // 551  unused[473]  -> total 1024
    put_u16(0,   0x0006);
    put_u16(2,   0x0001);
    put_u32(4,   1024);   // root page
    put_u32(8,   2048);   // next available
    put_u16(12,  4 + 8);  // item_size
    put_u16(14,  4);      // key_size
    put_u16(16,  0);      // key_dec
    put_u16(18,  4);      // max_item: room for 4 keys + 1 trailing
    put_u16(20,  2);      // half_page
    const char* expr = "TAG";
    std::memcpy(file.data() + 22, expr, std::strlen(expr));
    file[278] = 0;        // unique
    file[280] = 0;        // descend
    const char* tname = "T1";
    std::memcpy(file.data() + 538, tname, std::strlen(tname));

    // Page 1 (offset 1024, 1024 bytes): leaf with 2 keys
    std::size_t leaf = 1024;
    put_u16(leaf + 0, 2);                   // key_count

    // Offset table: slot i at leaf + 2 + i*2; need slots 0..max_item (5 slots)
    // Place key blocks immediately after offset table:
    std::size_t blocks_start = 2 + (4 + 1) * 2;   // 12
    std::size_t entry_size   = 4 + 4 + 4;          // left_child + recno + 4-byte key
    std::size_t blk0 = blocks_start;
    std::size_t blk1 = blocks_start + entry_size;
    std::size_t blk_tail = blocks_start + 2 * entry_size;     // trailing right-child slot

    put_u16(leaf + 2 + 0 * 2, static_cast<std::uint16_t>(blk0));
    put_u16(leaf + 2 + 1 * 2, static_cast<std::uint16_t>(blk1));
    put_u16(leaf + 2 + 2 * 2, static_cast<std::uint16_t>(blk_tail));
    put_u16(leaf + 2 + 3 * 2, 0);
    put_u16(leaf + 2 + 4 * 2, 0);

    // Key blocks: leaf has left_child = 0 in all entries.
    // Entry 0: key "AAAA" recno 1
    put_u32(leaf + blk0,     0);
    put_u32(leaf + blk0 + 4, 1);
    std::memcpy(file.data() + leaf + blk0 + 8, "AAAA", 4);
    // Entry 1: key "BBBB" recno 2
    put_u32(leaf + blk1,     0);
    put_u32(leaf + blk1 + 4, 2);
    std::memcpy(file.data() + leaf + blk1 + 8, "BBBB", 4);
    // Trailing slot (right child) = 0 since this is a leaf.
    put_u32(leaf + blk_tail, 0);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("NtxIndex opens and exposes the key expression and tag name") {
    auto p = make_simple_ntx("open");
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::ReadOnly).has_value());
        CHECK(ix.expression() == "TAG");
        CHECK(ix.name()       == "T1");
        CHECK(ix.key_length() == 4);
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
        CHECK(r.value().hit == SeekHit::Exact);
        CHECK(r.value().recno == 2);
        CHECK(ix.current_key() == "BBBB");
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex seek_last positions at the last key") {
    auto p = make_simple_ntx("last");
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::ReadOnly).has_value());
        auto r = ix.seek_last();
        REQUIRE(r.has_value());
        CHECK(r.value().recno == 2);
        CHECK(ix.current_key() == "BBBB");
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex prev walks backwards") {
    auto p = make_simple_ntx("prev");
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::ReadOnly).has_value());
        REQUIRE(ix.seek_last().has_value());
        auto r = ix.prev();
        REQUIRE(r.has_value());
        CHECK(r.value().positioned);
        CHECK(r.value().recno == 1);
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex create then insert + reopen finds the keys") {
    auto p = fs::temp_directory_path() / "openads_m3_ntx_create.ntx";
    fs::remove(p);
    {
        auto created = NtxIndex::create(p.string(), "T1", "TAG", 4, false, false);
        REQUIRE(created.has_value());
        NtxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "BBBB").has_value());
        REQUIRE(ix.insert(2, "AAAA").has_value());
        REQUIRE(ix.insert(3, "CCCC").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        auto a = ix.seek_first();
        REQUIRE(a.has_value());
        CHECK(ix.current_key() == "AAAA");
        REQUIRE(ix.next().has_value());
        CHECK(ix.current_key() == "BBBB");
        REQUIRE(ix.next().has_value());
        CHECK(ix.current_key() == "CCCC");
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex create with unique=true persists the flag through reopen") {
    auto p = fs::temp_directory_path() / "openads_m36_ntx_unique.ntx";
    fs::remove(p);
    {
        auto created = NtxIndex::create(p.string(), "T1", "TAG", 4, /*unique=*/true, false);
        REQUIRE(created.has_value());
        NtxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        CHECK(ix.unique());
        CHECK_FALSE(ix.descending());
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex create with descend=true persists the flag through reopen") {
    auto p = fs::temp_directory_path() / "openads_m36_ntx_descend.ntx";
    fs::remove(p);
    {
        auto created = NtxIndex::create(p.string(), "T1", "TAG", 4, false, /*descend=*/true);
        REQUIRE(created.has_value());
        NtxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        CHECK(ix.descending());
        CHECK_FALSE(ix.unique());
    }
    fs::remove(p);
}

TEST_CASE("NtxIndex erase removes a key") {
    auto p = fs::temp_directory_path() / "openads_m3_ntx_erase.ntx";
    fs::remove(p);
    {
        auto created = NtxIndex::create(p.string(), "T1", "TAG", 4, false, false);
        REQUIRE(created.has_value());
        NtxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        REQUIRE(ix.insert(2, "BBBB").has_value());
        REQUIRE(ix.insert(3, "CCCC").has_value());
        REQUIRE(ix.erase(2, "BBBB").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        NtxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        REQUIRE(ix.seek_first().has_value());
        CHECK(ix.current_key() == "AAAA");
        REQUIRE(ix.next().has_value());
        CHECK(ix.current_key() == "CCCC");
        auto end = ix.next();
        REQUIRE(end.has_value());
        CHECK_FALSE(end.value().positioned);
    }
    fs::remove(p);
}
