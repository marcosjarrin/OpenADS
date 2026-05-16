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
    // TAG is C(4); "AB" stored space-padded on disk -> AdsGetField must
    // return the full 4-char padded value so callers (e.g. xbrowse) see
    // the declared field width, not the trimmed length.
    CHECK(row1 == "AB  ");

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

// TDD: AdsGetField pads CHARACTER values to declared field width.
// DBF/xbase C(N) fields are fixed-width space-padded; FieldGet of a
// C(20) field with value "Alice" must return 20 chars, not 5.
TEST_CASE("AdsGetField pads CHARACTER field to declared width") {
    // Build a DBF in memory: C(20) field "NAME", 1 record with "Alice"
    // (5 chars) space-padded to 20 on disk.
    const auto dir = fs::temp_directory_path() / "openads_charpad";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    const int FW = 20; // field width
    auto p = dir / "charpad.dbf";
    {
        std::vector<std::uint8_t> file;
        std::array<std::uint8_t, 32> hdr{};
        hdr[0]  = 0x03;
        hdr[4]  = 1;                             // 1 record
        hdr[8]  = 32 + 32 + 1; hdr[9]  = 0;
        hdr[10] = static_cast<std::uint8_t>(1 + FW);
        hdr[11] = 0;
        file.insert(file.end(), hdr.begin(), hdr.end());
        std::array<std::uint8_t, 32> fd{};
        std::strncpy(reinterpret_cast<char*>(fd.data()), "NAME", 11);
        fd[11] = 'C';
        fd[16] = static_cast<std::uint8_t>(FW);
        file.insert(file.end(), fd.begin(), fd.end());
        file.push_back(0x0D);
        // Record: delete-flag + "Alice" + 15 spaces
        file.push_back(' ');
        const char* val = "Alice";
        for (int i = 0; i < FW; ++i)
            file.push_back(static_cast<std::uint8_t>(
                i < 5 ? static_cast<unsigned char>(val[i]) : ' '));
        file.push_back(0x1A);
        std::ofstream(p, std::ios::binary).write(
            reinterpret_cast<const char*>(file.data()),
            static_cast<std::streamsize>(file.size()));
    }

    ADSHANDLE hConn = 0;
    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER, nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 leaf[64] = "charpad.dbf";
    REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX, 0, 0, 0, 0, &hTable) == 0);
    REQUIRE(AdsGotoTop(hTable) == 0);

    UNSIGNED8 fld[16] = "NAME";
    UNSIGNED8 buf[64] = {0};
    UNSIGNED32 cap = sizeof(buf);
    REQUIRE(AdsGetField(hTable, fld, buf, &cap, 0) == 0);

    // cap must equal the declared field width (20), NOT the trimmed length (5).
    CHECK(cap == static_cast<UNSIGNED32>(FW));

    // The value must be "Alice" right-padded with spaces to 20 chars.
    std::string got(reinterpret_cast<const char*>(buf), cap);
    CHECK(got == std::string("Alice") + std::string(FW - 5, ' '));

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
