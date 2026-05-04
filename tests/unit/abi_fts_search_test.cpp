#include "doctest.h"
#include "openads/ace.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path stage_dbf(const fs::path& dir) {
    fs::create_directories(dir);
    auto p = dir / "data.dbf";
    fs::remove(p);
    std::vector<std::uint8_t> file;
    auto push = [&](const void* d, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(d);
        file.insert(file.end(), b, b + n);
    };
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 4;
    hdr[8]  = 32 + 32 + 1;
    hdr[10] = 1 + 50;
    push(hdr.data(), hdr.size());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "NOTES", 11);
    fd[11] = 'C'; fd[16] = 50;
    push(fd.data(), fd.size());
    file.push_back(0x0D);
    auto rec = [&](const char* s) {
        file.push_back(' ');
        for (int i = 0; i < 50; ++i)
            file.push_back(i < (int)std::strlen(s)
                           ? static_cast<std::uint8_t>(s[i]) : ' ');
    };
    rec("Quick brown fox jumps over lazy dog");
    rec("Lazy dog sleeps in sun");
    rec("Quick fox returns to den at dusk");
    rec("Brown bear eats honey from hive");
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

void make_fts(ADSHANDLE hConn) {
    UNSIGNED8 leaf[16] = "data";
    ADSHANDLE hTable = 0;
    REQUIRE(AdsOpenTable(hConn, leaf, leaf, ADS_CDX,
                         1, 1, 0, 1, &hTable) == 0);
    UNSIGNED8 file[64]  = "data.fts";
    UNSIGNED8 tag[16]   = "T";
    UNSIGNED8 field[16] = "NOTES";
    UNSIGNED8 nullbuf[2] = {0, 0};
    REQUIRE(AdsCreateFTSIndex(hTable, file, tag, field,
                              0, 3, 30,
                              1, nullbuf, 1, nullbuf, 1, nullbuf,
                              1, nullbuf, nullbuf, nullbuf, 0) == 0);
    REQUIRE(AdsCloseTable(hTable) == 0);
}

}  // namespace

TEST_CASE("M9.21 AdsFTSSearch single-token query returns matching recnos") {
    auto dir = fs::temp_directory_path() / "openads_m9_21_fts_search";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    make_fts(hConn);

    auto fts_path = (dir / "data.fts").string();
    UNSIGNED8 pathbuf[256];
    std::memcpy(pathbuf, fts_path.c_str(), fts_path.size() + 1);
    UNSIGNED8 query[64] = "fox";
    UNSIGNED32 recs[16] = {0};
    UNSIGNED32 cnt = 16;
    REQUIRE(AdsFTSSearch(hConn, pathbuf, query, recs, &cnt) == 0);
    CHECK(cnt == 2);
    std::set<UNSIGNED32> got{recs[0], recs[1]};
    CHECK(got == std::set<UNSIGNED32>{1, 3});

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.21 AdsFTSSearch multi-token AND-intersection") {
    auto dir = fs::temp_directory_path() / "openads_m9_21_fts_and";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    make_fts(hConn);

    auto fts_path = (dir / "data.fts").string();
    UNSIGNED8 pathbuf[256];
    std::memcpy(pathbuf, fts_path.c_str(), fts_path.size() + 1);

    // "lazy dog" appears together only in records 1 and 2.
    UNSIGNED8 q1[64] = "lazy dog";
    UNSIGNED32 r1[16] = {0};
    UNSIGNED32 c1 = 16;
    REQUIRE(AdsFTSSearch(hConn, pathbuf, q1, r1, &c1) == 0);
    CHECK(c1 == 2);
    std::set<UNSIGNED32> g1{r1[0], r1[1]};
    CHECK(g1 == std::set<UNSIGNED32>{1, 2});

    // "quick fox" in records 1 and 3.
    UNSIGNED8 q2[64] = "quick fox";
    UNSIGNED32 r2[16] = {0};
    UNSIGNED32 c2 = 16;
    REQUIRE(AdsFTSSearch(hConn, pathbuf, q2, r2, &c2) == 0);
    CHECK(c2 == 2);
    std::set<UNSIGNED32> g2{r2[0], r2[1]};
    CHECK(g2 == std::set<UNSIGNED32>{1, 3});

    // No record has both "fox" and "honey".
    UNSIGNED8 q3[64] = "fox honey";
    UNSIGNED32 r3[16] = {0};
    UNSIGNED32 c3 = 16;
    REQUIRE(AdsFTSSearch(hConn, pathbuf, q3, r3, &c3) == 0);
    CHECK(c3 == 0);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.21 AdsFTSSearch buffer truncation reports total count") {
    auto dir = fs::temp_directory_path() / "openads_m9_21_fts_trunc";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    make_fts(hConn);

    auto fts_path = (dir / "data.fts").string();
    UNSIGNED8 pathbuf[256];
    std::memcpy(pathbuf, fts_path.c_str(), fts_path.size() + 1);

    // "lazy" hits records 1 and 2 → request capacity 1 → out-count
    // still reports 2 even though only 1 recno was copied.
    UNSIGNED8 q[64] = "lazy";
    UNSIGNED32 r[1] = {0};
    UNSIGNED32 c = 1;
    REQUIRE(AdsFTSSearch(hConn, pathbuf, q, r, &c) == 0);
    CHECK(c == 2);
    CHECK((r[0] == 1 || r[0] == 2));

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.21 AdsFTSSearch on missing file returns error") {
    UNSIGNED8 path[64] = "C:\\does\\not\\exist.fts";
    UNSIGNED8 q[16]    = "x";
    UNSIGNED32 r[1]    = {0};
    UNSIGNED32 c       = 1;
    UNSIGNED32 rc = AdsFTSSearch(/*hConn=*/0, path, q, r, &c);
    CHECK(rc != 0);
}
