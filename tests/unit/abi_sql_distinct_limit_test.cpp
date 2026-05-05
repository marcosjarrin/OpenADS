#include "doctest.h"
#include "openads/ace.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void write_dbf(const fs::path& path,
               const std::vector<std::pair<std::string, std::uint8_t>>& cols,
               const std::vector<std::vector<std::string>>& rows) {
    std::vector<std::uint8_t> file;
    auto push = [&](const void* d, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(d);
        file.insert(file.end(), b, b + n);
    };
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03;
    hdr[4] = static_cast<std::uint8_t>(rows.size());
    std::uint16_t hl = static_cast<std::uint16_t>(32 + 32 * cols.size() + 1);
    std::uint16_t rl = 1;
    for (auto& c : cols) rl += c.second;
    hdr[8]  = static_cast<std::uint8_t>( hl       & 0xFFu);
    hdr[9]  = static_cast<std::uint8_t>((hl >> 8) & 0xFFu);
    hdr[10] = static_cast<std::uint8_t>( rl       & 0xFFu);
    hdr[11] = static_cast<std::uint8_t>((rl >> 8) & 0xFFu);
    push(hdr.data(), hdr.size());
    for (auto& c : cols) {
        std::array<std::uint8_t, 32> fd{};
        std::strncpy(reinterpret_cast<char*>(fd.data()),
                     c.first.c_str(), 11);
        fd[11] = 'C'; fd[16] = c.second;
        push(fd.data(), fd.size());
    }
    file.push_back(0x0D);
    for (auto& row : rows) {
        file.push_back(' ');
        for (std::size_t i = 0; i < cols.size(); ++i) {
            const auto& v = row[i];
            std::uint8_t L = cols[i].second;
            for (std::uint8_t k = 0; k < L; ++k) {
                file.push_back(k < v.size()
                    ? static_cast<std::uint8_t>(v[k]) : ' ');
            }
        }
    }
    file.push_back(0x1A);
    std::ofstream(path, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
}

}  // namespace

TEST_CASE("M10.31 SELECT DISTINCT dedups by projected columns") {
    auto dir = fs::temp_directory_path() / "openads_m10_31";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf",
        {{"CITY", 4}, {"AMT", 4}},
        {{"NYC ", "10  "},
         {"NYC ", "20  "},
         {"LON ", "30  "},
         {"NYC ", "10  "},                            // duplicate of row 1
         {"PAR ", "5   "}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[200] = "SELECT DISTINCT CITY FROM data.dbf";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 3);                                  // NYC, LON, PAR

    UNSIGNED8 sql2[200] = "SELECT DISTINCT * FROM data.dbf";
    ADSHANDLE hCur2 = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql2, &hCur2) == 0);
    UNSIGNED32 cnt2 = 0;
    REQUIRE(AdsGetRecordCount(hCur2, 0, &cnt2) == 0);
    CHECK(cnt2 == 4);                                 // 5 rows minus 1 dup

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.32 LIMIT and OFFSET slice the result") {
    auto dir = fs::temp_directory_path() / "openads_m10_32";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf",
        {{"TAG", 1}},
        {{"A"}, {"B"}, {"C"}, {"D"}, {"E"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[160] = "SELECT * FROM data.dbf LIMIT 2";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 2);

    UNSIGNED8 sql2[160] = "SELECT * FROM data.dbf LIMIT 2 OFFSET 2";
    ADSHANDLE hCur2 = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql2, &hCur2) == 0);
    UNSIGNED32 cnt2 = 0;
    REQUIRE(AdsGetRecordCount(hCur2, 0, &cnt2) == 0);
    CHECK(cnt2 == 2);

    UNSIGNED8 sql3[160] = "SELECT * FROM data.dbf LIMIT 100 OFFSET 4";
    ADSHANDLE hCur3 = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql3, &hCur3) == 0);
    UNSIGNED32 cnt3 = 0;
    REQUIRE(AdsGetRecordCount(hCur3, 0, &cnt3) == 0);
    CHECK(cnt3 == 1);                                 // only "E"

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.33 BETWEEN keeps inclusive bounds (string)") {
    auto dir = fs::temp_directory_path() / "openads_m10_33_bw";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf",
        {{"TAG", 1}},
        {{"A"}, {"B"}, {"C"}, {"D"}, {"E"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[200] =
        "SELECT * FROM data.dbf WHERE TAG BETWEEN 'B' AND 'D'";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 3);                                  // B, C, D

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.33 LIKE matches glob pattern") {
    auto dir = fs::temp_directory_path() / "openads_m10_33_like";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf",
        {{"NAME", 8}},
        {{"Alice"}, {"Albert"}, {"Bob"}, {"Charlie"}, {"Alpha"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    // NAME starts with 'Al'.
    UNSIGNED8 sql[200] = "SELECT * FROM data.dbf WHERE NAME LIKE 'Al%'";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 3);                                  // Alice, Albert, Alpha

    // NAME has 'b' anywhere.
    UNSIGNED8 sql2[200] = "SELECT * FROM data.dbf WHERE NAME LIKE '%b%'";
    ADSHANDLE hCur2 = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql2, &hCur2) == 0);
    UNSIGNED32 cnt2 = 0;
    REQUIRE(AdsGetRecordCount(hCur2, 0, &cnt2) == 0);
    CHECK(cnt2 == 2);                                 // Albert, Bob

    // 5-char names exactly.
    UNSIGNED8 sql3[200] = "SELECT * FROM data.dbf WHERE NAME LIKE '_____'";
    ADSHANDLE hCur3 = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql3, &hCur3) == 0);
    UNSIGNED32 cnt3 = 0;
    REQUIRE(AdsGetRecordCount(hCur3, 0, &cnt3) == 0);
    CHECK(cnt3 == 2);                                 // Alice, Alpha

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
