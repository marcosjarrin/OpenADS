#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path make_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    fs::remove(p);

    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 2; // 2 records
    hdr[8]  = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 4;       hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());

    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C';
    fd[16] = 4;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);

    auto push_rec = [&](const char* name) {
        file.push_back(' ');
        for (int i = 0; i < 4; ++i) {
            file.push_back(static_cast<std::uint8_t>(
                i < static_cast<int>(std::strlen(name)) ? name[i] : ' '));
        }
    };
    push_rec("AB");
    push_rec("CDEF");
    file.push_back(0x1A);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("ABI smoke: open dir, open table, walk records, read field") {
    const auto dir = fs::temp_directory_path() / "openads_m1_abi";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir, "data.dbf");

    ADSHANDLE hConn = 0;
    UNSIGNED8 server_buf[256];
    std::memcpy(server_buf, dir.string().c_str(), dir.string().size() + 1);
    REQUIRE(AdsConnect60(server_buf, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name_buf[64] = "data.dbf";
    REQUIRE(AdsOpenTable(hConn, name_buf, nullptr, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    UNSIGNED16 fields = 0;
    REQUIRE(AdsGetNumFields(hTable, &fields) == 0);
    CHECK(fields == 1);

    UNSIGNED32 rec_count = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &rec_count) == 0);
    CHECK(rec_count == 2);

    REQUIRE(AdsGotoTop(hTable) == 0);

    auto read_first_field = [&](std::string& out) {
        UNSIGNED8 fname[64] = "TAG";
        UNSIGNED8 buf[64]   = {0};
        UNSIGNED32 cap = sizeof(buf);
        UNSIGNED32 r = AdsGetField(hTable, fname, buf, &cap, 0);
        REQUIRE(r == 0);
        out.assign(reinterpret_cast<const char*>(buf), cap);
    };

    std::string row1;
    read_first_field(row1);
    CHECK(row1 == "AB");

    REQUIRE(AdsSkip(hTable, 1) == 0);
    std::string row2;
    read_first_field(row2);
    CHECK(row2 == "CDEF");

    REQUIRE(AdsSkip(hTable, 1) == 0);
    UNSIGNED16 at_eof = 0;
    REQUIRE(AdsAtEOF(hTable, &at_eof) == 0);
    CHECK(at_eof == 1);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);

    fs::remove_all(dir, ec);
}
