#include "doctest.h"
#include "openads/ace.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

void write_dbf_typed(const fs::path& path,
                     const std::vector<std::tuple<std::string, char,
                                                   std::uint8_t>>& cols,
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
    for (auto& c : cols) rl += std::get<2>(c);
    hdr[8]  = static_cast<std::uint8_t>( hl       & 0xFFu);
    hdr[9]  = static_cast<std::uint8_t>((hl >> 8) & 0xFFu);
    hdr[10] = static_cast<std::uint8_t>( rl       & 0xFFu);
    hdr[11] = static_cast<std::uint8_t>((rl >> 8) & 0xFFu);
    push(hdr.data(), hdr.size());
    for (auto& c : cols) {
        std::array<std::uint8_t, 32> fd{};
        std::strncpy(reinterpret_cast<char*>(fd.data()),
                     std::get<0>(c).c_str(), 11);
        fd[11] = static_cast<std::uint8_t>(std::get<1>(c));
        fd[16] = std::get<2>(c);
        push(fd.data(), fd.size());
    }
    file.push_back(0x0D);
    for (auto& row : rows) {
        file.push_back(' ');
        for (std::size_t i = 0; i < cols.size(); ++i) {
            const auto& v = row[i];
            std::uint8_t L = std::get<2>(cols[i]);
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

std::string read_field(ADSHANDLE hCur, const char* name) {
    UNSIGNED8 buf[64] = {0};
    UNSIGNED32 cap = sizeof(buf);
    REQUIRE(AdsGetField(hCur, (UNSIGNED8*)name, buf, &cap, 0) == 0);
    std::string s((char*)buf, cap);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

}  // namespace

TEST_CASE("M10.39 UPPER / LOWER / LEN / TRIM in projection") {
    auto dir = fs::temp_directory_path() / "openads_m10_39";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf_typed(dir / "data.dbf",
        {{"NAME", 'C', 8}},
        {{"  bob  "},
         {"alice"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[300] =
        "SELECT UPPER(NAME) AS U, LOWER(NAME) AS L, "
        "TRIM(NAME) AS T, LEN(NAME) AS N FROM data.dbf";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    REQUIRE(AdsGotoTop(hCur) == 0);
    CHECK(read_field(hCur, "U") == "  BOB");
    CHECK(read_field(hCur, "L") == "  bob");
    CHECK(read_field(hCur, "T") == "bob");
    CHECK(read_field(hCur, "N") == "5");                  // "  bob" len-trim-right

    REQUIRE(AdsSkip(hCur, 1) == 0);
    CHECK(read_field(hCur, "U") == "ALICE");
    CHECK(read_field(hCur, "T") == "alice");
    CHECK(read_field(hCur, "N") == "5");

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M10.40 arithmetic in projection") {
    auto dir = fs::temp_directory_path() / "openads_m10_40";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    write_dbf_typed(dir / "data.dbf",
        {{"A", 'N', 4}, {"B", 'N', 4}},
        {{"  10", "   3"},
         {"  20", "   5"}});

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    ADSHANDLE hStmt = 0;
    REQUIRE(AdsCreateSQLStatement(hConn, &hStmt) == 0);

    UNSIGNED8 sql[260] =
        "SELECT A + B AS S, A * 2 AS D, A - B AS X FROM data.dbf";
    ADSHANDLE hCur = 0;
    REQUIRE(AdsExecuteSQLDirect(hStmt, sql, &hCur) == 0);
    REQUIRE(AdsGotoTop(hCur) == 0);
    CHECK(read_field(hCur, "S") == "13");
    CHECK(read_field(hCur, "D") == "20");
    CHECK(read_field(hCur, "X") == "7");

    REQUIRE(AdsSkip(hCur, 1) == 0);
    CHECK(read_field(hCur, "S") == "25");
    CHECK(read_field(hCur, "D") == "40");
    CHECK(read_field(hCur, "X") == "15");

    REQUIRE(AdsCloseSQLStatement(hStmt) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
