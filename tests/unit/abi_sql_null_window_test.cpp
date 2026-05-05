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

TEST_CASE("M10.44 IS NULL / IS NOT NULL filters all-blanks rows") {
    auto dir = fs::temp_directory_path() / "openads_m10_44";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf", {{"NAME", 5}},
        {{"Alice"}, {"     "}, {"Bob"}, {""}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[200] =
        "SELECT * FROM data.dbf WHERE NAME IS NULL";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 2);                          // 2 all-blank rows

    UNSIGNED8 sql2[200] =
        "SELECT * FROM data.dbf WHERE NAME IS NOT NULL";
    ADSHANDLE hCur2 = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql2, &hCur2) == 0);
    UNSIGNED32 cnt2 = 0;
    REQUIRE(AdsGetRecordCount(hCur2, 0, &cnt2) == 0);
    CHECK(cnt2 == 2);                         // Alice + Bob

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.49 / M10.50 ROW_NUMBER / RANK / DENSE_RANK with PARTITION BY + ORDER BY") {
    auto dir = fs::temp_directory_path() / "openads_m10_49_50";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    // 2 partitions (CITY) × scores. RANK skips on ties, DENSE_RANK
    // doesn't.
    write_dbf(dir / "data.dbf",
        {{"CITY", 4}, {"SCORE", 3}},
        {{"NYC ", " 90"},
         {"NYC ", " 85"},
         {"NYC ", " 85"},                           // tie with row 2
         {"NYC ", " 70"},
         {"LON ", " 95"},
         {"LON ", " 80"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[300] =
        "SELECT CITY, SCORE, "
        "ROW_NUMBER() OVER (PARTITION BY CITY ORDER BY SCORE DESC) AS RN, "
        "RANK()       OVER (PARTITION BY CITY ORDER BY SCORE DESC) AS RK, "
        "DENSE_RANK() OVER (PARTITION BY CITY ORDER BY SCORE DESC) AS DR "
        "FROM data.dbf";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);

    auto read = [&](const char* name) {
        UNSIGNED8 buf[16] = {0};
        UNSIGNED32 cap = sizeof(buf);
        REQUIRE(AdsGetField(hCur, (UNSIGNED8*)name, buf, &cap, 0) == 0);
        std::string s((char*)buf, cap);
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };
    // Source order preserved (no outer ORDER BY); window values
    // computed per partition.
    REQUIRE(AdsGotoTop(hCur) == 0);   // recno 1: NYC 90 → RN=1, RK=1, DR=1
    CHECK(read("RN") == "1");
    CHECK(read("RK") == "1");
    CHECK(read("DR") == "1");
    REQUIRE(AdsSkip(hCur, 1) == 0);   // recno 2: NYC 85 → RN=2, RK=2, DR=2
    CHECK(read("RN") == "2");
    CHECK(read("RK") == "2");
    CHECK(read("DR") == "2");
    REQUIRE(AdsSkip(hCur, 1) == 0);   // recno 3: NYC 85 (tie) → RN=3, RK=2, DR=2
    CHECK(read("RN") == "3");
    CHECK(read("RK") == "2");
    CHECK(read("DR") == "2");
    REQUIRE(AdsSkip(hCur, 1) == 0);   // recno 4: NYC 70 → RN=4, RK=4, DR=3
    CHECK(read("RN") == "4");
    CHECK(read("RK") == "4");
    CHECK(read("DR") == "3");
    REQUIRE(AdsSkip(hCur, 1) == 0);   // recno 5: LON 95 → RN=1, RK=1, DR=1
    CHECK(read("RN") == "1");
    CHECK(read("RK") == "1");
    CHECK(read("DR") == "1");
    REQUIRE(AdsSkip(hCur, 1) == 0);   // recno 6: LON 80 → RN=2, RK=2, DR=2
    CHECK(read("RN") == "2");

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.51 qualified column refs (alias.col)") {
    auto dir = fs::temp_directory_path() / "openads_m10_51";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf", {{"NAME", 5}}, {{"Alice"}, {"Bob"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    // Use `t.NAME` qualifier; engine drops the prefix and resolves
    // `NAME` against the cursor.
    UNSIGNED8 sql[200] =
        "SELECT t.NAME FROM data.dbf WHERE t.NAME = 'Alice'";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 1);

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.52 INSERT INTO ... VALUES multi-row") {
    auto dir = fs::temp_directory_path() / "openads_m10_52";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "dst.dbf", {{"TAG", 4}}, {});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[300] =
        "INSERT INTO dst.dbf (TAG) VALUES "
        "('AAAA'), ('BBBB'), ('CCCC')";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    CHECK(hCur == 0);

    UNSIGNED8 sel[200] = "SELECT * FROM dst.dbf";
    REQUIRE(AdsExecuteSQLDirect(hStmt, sel, &hCur) == 0);
    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hCur, 0, &cnt) == 0);
    CHECK(cnt == 3);

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.47 ROW_NUMBER() OVER () in projection") {
    auto dir = fs::temp_directory_path() / "openads_m10_47";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf(dir / "data.dbf", {{"TAG", 4}},
        {{"AAAA"}, {"BBBB"}, {"CCCC"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[260] =
        "SELECT TAG, ROW_NUMBER() OVER () AS RN FROM data.dbf";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);

    auto read = [&](const char* name) {
        UNSIGNED8 buf[16] = {0};
        UNSIGNED32 cap = sizeof(buf);
        REQUIRE(AdsGetField(hCur, (UNSIGNED8*)name, buf, &cap, 0) == 0);
        std::string s((char*)buf, cap);
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };
    REQUIRE(AdsGotoTop(hCur) == 0);
    CHECK(read("RN") == "1");
    REQUIRE(AdsSkip(hCur, 1) == 0);
    CHECK(read("RN") == "2");
    REQUIRE(AdsSkip(hCur, 1) == 0);
    CHECK(read("RN") == "3");

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
