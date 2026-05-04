#include "doctest.h"
#include "openads/ace.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path stage_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    fs::remove(p);
    std::vector<std::uint8_t> file;
    auto push = [&](const void* d, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(d);
        file.insert(file.end(), b, b + n);
    };
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03; hdr[4]  = 4;
    hdr[8]  = 32 + 32 + 1; hdr[10] = 1 + 4;
    push(hdr.data(), hdr.size());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C'; fd[16] = 4;
    push(fd.data(), fd.size());
    file.push_back(0x0D);
    auto rec = [&](std::uint8_t del, const char* s) {
        file.push_back(del);
        for (int i = 0; i < 4; ++i)
            file.push_back(i < (int)std::strlen(s)
                           ? static_cast<std::uint8_t>(s[i]) : ' ');
    };
    rec(' ', "AAAA");
    rec('*', "BBBB");
    rec(' ', "CCCC");
    rec(' ', "DDDD");
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("M9.11 AdsCopyTable copies live records to a new DBF") {
    auto dir = fs::temp_directory_path() / "openads_m911_copy";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir, "src.dbf");

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hSrc = 0;
    UNSIGNED8 src_name[16] = "src";
    REQUIRE(AdsOpenTable(hConn, src_name, src_name,
                         ADS_CDX, 1, 1, 0, 1, &hSrc) == 0);

    UNSIGNED8 target[64] = "dst.dbf";
    REQUIRE(AdsCopyTable(hSrc, /*ADS_RESPECTFILTERS*/ 1, target) == 0);

    REQUIRE(AdsCloseTable(hSrc) == 0);

    // Open the copy and verify the live records survived (3 not 4
    // because the deleted BBBB row was filtered out).
    ADSHANDLE hDst = 0;
    UNSIGNED8 dst_name[16] = "dst";
    REQUIRE(AdsOpenTable(hConn, dst_name, dst_name,
                         ADS_CDX, 1, 1, 0, 1, &hDst) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hDst, 0, &cnt) == 0);
    CHECK(cnt == 3u);

    REQUIRE(AdsGotoRecord(hDst, 1) == 0);
    UNSIGNED8 fld[16] = "TAG";
    UNSIGNED8 buf[16] = {0};
    UNSIGNED32 cap = sizeof(buf);
    REQUIRE(AdsGetField(hDst, fld, buf, &cap, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "AAAA");

    REQUIRE(AdsGotoRecord(hDst, 2) == 0);
    cap = sizeof(buf); std::memset(buf, 0, sizeof(buf));
    REQUIRE(AdsGetField(hDst, fld, buf, &cap, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "CCCC");

    REQUIRE(AdsCloseTable(hDst) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.11 AdsCopyTableContents appends src rows into dst") {
    auto dir = fs::temp_directory_path() / "openads_m911_contents";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir, "src.dbf");

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    // Stage an empty dst table with the same schema via AdsCreateTable.
    UNSIGNED8 dst_name[64] = "dst";
    UNSIGNED8 dst_alias[64]= "dst";
    UNSIGNED8 fields[256]  = "TAG,Character,4";
    ADSHANDLE hDst = 0;
    REQUIRE(AdsCreateTable(hConn, dst_name, dst_alias,
                           ADS_CDX, 0, 0, 0, 64,
                           fields, &hDst) == 0);

    ADSHANDLE hSrc = 0;
    UNSIGNED8 src_name[16] = "src";
    REQUIRE(AdsOpenTable(hConn, src_name, src_name,
                         ADS_CDX, 1, 1, 0, 1, &hSrc) == 0);

    REQUIRE(AdsCopyTableContents(hSrc, hDst) == 0);

    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hDst, 0, &cnt) == 0);
    CHECK(cnt == 3u);   // BBBB filtered as deleted

    REQUIRE(AdsCloseTable(hSrc) == 0);
    REQUIRE(AdsCloseTable(hDst) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
