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

fs::path make_dbf(const fs::path& dir) {
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
    hdr[10] = 1 + 4;
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
    rec("ALPH"); rec("BETA"); rec("GAMM"); rec("DELT");
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

std::set<UNSIGNED32> walk(ADSHANDLE hCur) {
    std::set<UNSIGNED32> out;
    if (AdsGotoTop(hCur) != 0) return out;
    while (true) {
        UNSIGNED16 atend = 0;
        if (AdsAtEOF(hCur, &atend) != 0) break;
        if (atend) break;
        UNSIGNED32 r = 0;
        if (AdsGetRecordNum(hCur, 0, &r) != 0) break;
        out.insert(r);
        if (AdsSkip(hCur, 1) != 0) break;
    }
    return out;
}

}  // namespace

TEST_CASE("M10.3 SQL WHERE A = 'x' OR A = 'y' returns the union") {
    auto dir = fs::temp_directory_path() / "openads_m10_3_or";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[160] =
        "SELECT * FROM data.dbf WHERE TAG = 'ALPH' OR TAG = 'GAMM'";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);

    auto got = walk(hCur);
    CHECK(got == std::set<UNSIGNED32>{1, 3});

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.3 SQL WHERE NOT TAG = 'x' inverts the predicate") {
    auto dir = fs::temp_directory_path() / "openads_m10_3_not";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[128] = "SELECT * FROM data.dbf WHERE NOT TAG = 'BETA'";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);

    auto got = walk(hCur);
    CHECK(got == std::set<UNSIGNED32>{1, 3, 4});

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.3 SQL WHERE (A OR B) AND C combines parens correctly") {
    auto dir = fs::temp_directory_path() / "openads_m10_3_paren";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    // (TAG='ALPH' OR TAG='BETA') AND TAG <> 'BETA' → only rec 1 (ALPH).
    UNSIGNED8 sql[160] =
        "SELECT * FROM data.dbf WHERE "
        "(TAG = 'ALPH' OR TAG = 'BETA') AND TAG <> 'BETA'";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);

    auto got = walk(hCur);
    CHECK(got == std::set<UNSIGNED32>{1});

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
