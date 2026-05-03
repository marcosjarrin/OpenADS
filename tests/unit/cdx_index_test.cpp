#include "doctest.h"
#include "drivers/cdx/cdx_index.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using openads::drivers::IndexOpenMode;
using openads::drivers::SeekHit;
using openads::drivers::cdx::CdxIndex;

TEST_CASE("CdxIndex create + insert + reopen walks keys in compact-leaf order") {
    auto p = fs::temp_directory_path() / "openads_m35_cdx_basic.cdx";
    fs::remove(p);
    {
        auto created = CdxIndex::create(p.string(), "T1", "TAG", 4, false, false);
        REQUIRE(created.has_value());
        CdxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(2, "AAAA").has_value());
        REQUIRE(ix.insert(3, "BBBB").has_value());
        REQUIRE(ix.insert(1, "CCCC").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        CdxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        CHECK(ix.expression() == "TAG");
        CHECK(ix.name()       == "T1");

        auto a = ix.seek_first();
        REQUIRE(a.has_value());
        CHECK(a.value().recno == 2);
        CHECK(ix.current_key() == "AAAA");

        REQUIRE(ix.next().has_value());
        CHECK(ix.current_key() == "BBBB");
        REQUIRE(ix.next().has_value());
        CHECK(ix.current_key() == "CCCC");
        auto end = ix.next();
        REQUIRE(end.has_value());
        CHECK_FALSE(end.value().positioned);
    }
    fs::remove(p);
}

TEST_CASE("CdxIndex seek_key locates an exact match") {
    auto p = fs::temp_directory_path() / "openads_m35_cdx_seek.cdx";
    fs::remove(p);
    {
        auto created = CdxIndex::create(p.string(), "T1", "TAG", 4, false, false);
        REQUIRE(created.has_value());
        CdxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        REQUIRE(ix.insert(2, "BBBB").has_value());
        REQUIRE(ix.insert(3, "CCCC").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        CdxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        auto r = ix.seek_key("BBBB", false);
        REQUIRE(r.has_value());
        CHECK(r.value().hit == SeekHit::Exact);
        CHECK(r.value().recno == 2);
        CHECK(ix.current_key() == "BBBB");
    }
    fs::remove(p);
}

TEST_CASE("CdxIndex seek_last + prev walks backward") {
    auto p = fs::temp_directory_path() / "openads_m35_cdx_back.cdx";
    fs::remove(p);
    {
        auto created = CdxIndex::create(p.string(), "T1", "TAG", 4, false, false);
        REQUIRE(created.has_value());
        CdxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        REQUIRE(ix.insert(2, "BBBB").has_value());
        REQUIRE(ix.insert(3, "CCCC").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        CdxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        auto r = ix.seek_last();
        REQUIRE(r.has_value());
        CHECK(ix.current_key() == "CCCC");
        REQUIRE(ix.prev().has_value());
        CHECK(ix.current_key() == "BBBB");
        REQUIRE(ix.prev().has_value());
        CHECK(ix.current_key() == "AAAA");
        auto begin = ix.prev();
        REQUIRE(begin.has_value());
        CHECK_FALSE(begin.value().positioned);
    }
    fs::remove(p);
}

TEST_CASE("CdxIndex erase removes a key") {
    auto p = fs::temp_directory_path() / "openads_m35_cdx_erase.cdx";
    fs::remove(p);
    {
        auto created = CdxIndex::create(p.string(), "T1", "TAG", 4, false, false);
        REQUIRE(created.has_value());
        CdxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        REQUIRE(ix.insert(2, "BBBB").has_value());
        REQUIRE(ix.insert(3, "CCCC").has_value());
        REQUIRE(ix.erase(2, "BBBB").has_value());
        REQUIRE(ix.flush().has_value());
    }
    {
        CdxIndex ix;
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

TEST_CASE("CdxIndex compound layout: file header + struct-tag leaf + sub-tag header") {
    auto p = fs::temp_directory_path() / "openads_m39_cdx_compound.cdx";
    fs::remove(p);
    {
        auto created = CdxIndex::create(p.string(), "MYTAG", "FIELD", 8, false, false);
        REQUIRE(created.has_value());
        CdxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "alpha").has_value());
        REQUIRE(ix.insert(2, "beta").has_value());
        REQUIRE(ix.flush().has_value());
    }

    // Direct on-disk inspection: file header at 0, struct-tag root leaf at
    // offset 1024 mapping the tag name to 1536, sub-tag header at 1536.
    {
        std::ifstream f(p, std::ios::binary);
        REQUIRE(f.is_open());
        std::uint8_t hdr[1024]{};
        f.read(reinterpret_cast<char*>(hdr), 1024);
        auto rd32 = [](const std::uint8_t* x) {
            return  static_cast<std::uint32_t>(x[0])        |
                   (static_cast<std::uint32_t>(x[1]) <<  8) |
                   (static_cast<std::uint32_t>(x[2]) << 16) |
                   (static_cast<std::uint32_t>(x[3]) << 24);
        };
        auto rd16 = [](const std::uint8_t* x) {
            return  static_cast<std::uint16_t>(x[0]) |
                   (static_cast<std::uint16_t>(x[1]) << 8);
        };
        CHECK(rd32(hdr + 0) == 1024u);                 // struct-tag root
        CHECK(rd16(hdr + 12) == 10u);                  // struct-tag key_size = 10
        CHECK(static_cast<int>(hdr[14] & 0x40) != 0);  // CDX_TYPE_COMPOUND bit

        f.seekg(1024);
        std::uint8_t leaf[512]{};
        f.read(reinterpret_cast<char*>(leaf), 512);
        CHECK(rd16(leaf + 0) == 2u);                   // CDX_NODE_LEAF
        CHECK(rd16(leaf + 2) == 1u);                   // one entry

        f.seekg(1536);
        std::uint8_t sub[1024]{};
        f.read(reinterpret_cast<char*>(sub), 1024);
        CHECK(rd16(sub + 12) == 8u);                   // sub-tag key_size
    }

    // Reopen via the public API and confirm the sub-tag walks correctly.
    {
        CdxIndex ix;
        REQUIRE(ix.open(p.string(), IndexOpenMode::Shared).has_value());
        CHECK(ix.name() == "MYTAG");
        CHECK(ix.expression() == "FIELD");
        CHECK(ix.key_length() == 8);
        REQUIRE(ix.seek_first().has_value());
        CHECK(ix.current_key() == "alpha   ");     // padded to 8
        REQUIRE(ix.next().has_value());
        CHECK(ix.current_key() == "beta    ");
    }
    fs::remove(p);
}

TEST_CASE("CdxIndex unique tag rejects duplicates") {
    auto p = fs::temp_directory_path() / "openads_m35_cdx_unique.cdx";
    fs::remove(p);
    {
        auto created = CdxIndex::create(p.string(), "T1", "TAG", 4, true, false);
        REQUIRE(created.has_value());
        CdxIndex ix = std::move(created).value();
        REQUIRE(ix.insert(1, "AAAA").has_value());
        auto r = ix.insert(2, "AAAA");
        CHECK_FALSE(r.has_value());
        CHECK(r.error().code == 5044);
    }
    fs::remove(p);
}
