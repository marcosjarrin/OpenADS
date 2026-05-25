#include "doctest.h"
#include "openads/ace.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path make_enum_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    fs::remove(p);
    std::vector<std::uint8_t> file;
    auto push = [&](const void* d, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(d);
        file.insert(file.end(), b, b + n);
    };
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03; hdr[4] = 2;
    hdr[8] = 32 + 32 + 1; hdr[10] = 1 + 4;
    push(hdr.data(), hdr.size());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C'; fd[16] = 4;
    push(fd.data(), fd.size());
    file.push_back(0x0D);
    auto rec = [&](const char* s) {
        file.push_back(' ');
        for (int i = 0; i < 4; ++i)
            file.push_back(i < (int)std::strlen(s)
                           ? static_cast<std::uint8_t>(s[i]) : ' ');
    };
    rec("AAAA"); rec("BBBB");
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("AdsGetAllTables enumerates tables open on a connection") {
    auto dir = fs::temp_directory_path() / "openads_enum_tables";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_enum_dbf(dir, "t1.dbf");
    make_enum_dbf(dir, "t2.dbf");

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hT1 = 0, hT2 = 0;
    UNSIGNED8 n1[8] = "t1", n2[8] = "t2";
    REQUIRE(AdsOpenTable(hConn, n1, n1, ADS_CDX, 0, 0, 0, 0, &hT1) == 0);
    REQUIRE(AdsOpenTable(hConn, n2, n2, ADS_CDX, 0, 0, 0, 0, &hT2) == 0);

    std::array<ADSHANDLE, 8> arr{};
    UNSIGNED16 len = 8;
    REQUIRE(AdsGetAllTables(hConn, arr.data(), &len) == 0);
    CHECK(len == 2u);
    bool saw_t1 = std::find(arr.begin(), arr.begin() + len, hT1) != arr.begin() + len;
    bool saw_t2 = std::find(arr.begin(), arr.begin() + len, hT2) != arr.begin() + len;
    CHECK(saw_t1);
    CHECK(saw_t2);

    // Capacity clipping: ask for only 1 slot → count reported is 1.
    std::array<ADSHANDLE, 1> small{};
    UNSIGNED16 slen = 1;
    REQUIRE(AdsGetAllTables(hConn, small.data(), &slen) == 0);
    CHECK(slen == 1u);

    // After closing one table the count drops.
    REQUIRE(AdsCloseTable(hT1) == 0);
    UNSIGNED16 after = 8;
    REQUIRE(AdsGetAllTables(hConn, arr.data(), &after) == 0);
    CHECK(after == 1u);
    CHECK(arr[0] == hT2);

    REQUIRE(AdsCloseTable(hT2) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsGetAllIndexes enumerates open indexes on a table") {
    auto dir = fs::temp_directory_path() / "openads_enum_indexes";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_enum_dbf(dir, "data.dbf");

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 leaf[16] = "data.dbf";
    REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    // No indexes open yet.
    std::array<ADSHANDLE, 8> arr{};
    UNSIGNED16 len = 8;
    REQUIRE(AdsGetAllIndexes(hTable, arr.data(), &len) == 0);
    CHECK(len == 0u);

    // Create an NTX index.
    auto idx_path = (dir / "data.ntx").string();
    UNSIGNED8 idx_buf[260];
    std::memcpy(idx_buf, idx_path.c_str(), idx_path.size() + 1);
    UNSIGNED8 tag[8]  = "T1";
    UNSIGNED8 expr[8] = "TAG";
    ADSHANDLE hIdx = 0;
    REQUIRE(AdsCreateIndex(hTable, idx_buf, tag, expr, nullptr, 0, 0, &hIdx)
            == 0);

    // Now the index appears in the enumeration.
    len = 8;
    REQUIRE(AdsGetAllIndexes(hTable, arr.data(), &len) == 0);
    CHECK(len == 1u);
    CHECK(arr[0] == hIdx);

    // Count-only call (null array pointer) still updates len.
    UNSIGNED16 cnt = 8;
    REQUIRE(AdsGetAllIndexes(hTable, nullptr, &cnt) == 0);
    CHECK(cnt == 1u);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsGetFTSIndexes returns zero (no persistent FTS handles)") {
    auto dir = fs::temp_directory_path() / "openads_enum_fts";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_enum_dbf(dir, "data.dbf");

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 leaf[16] = "data.dbf";
    REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    std::array<ADSHANDLE, 8> arr{};
    UNSIGNED16 len = 8;
    REQUIRE(AdsGetFTSIndexes(hTable, arr.data(), &len) == 0);
    CHECK(len == 0u);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
