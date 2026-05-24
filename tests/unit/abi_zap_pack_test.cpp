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

// Build a minimal ADT file (400-byte header + one CHAR(4) field + N records).
// Records: active=0x04, deleted=0x05; rec_len = 1 status + 4 null-bitmap + 4 data = 9.
fs::path stage_adt(const fs::path& dir) {
    fs::create_directories(dir);
    auto p = dir / "data.adt";
    fs::remove(p);

    const std::uint32_t HDR_LEN = 600;  // 400 + 1*200
    const std::uint32_t REC_LEN = 9;    // 1 + 4 + 4
    const std::uint32_t NUM_RECS = 4;

    std::vector<std::uint8_t> file(HDR_LEN, 0);

    auto put_u32 = [&](std::size_t off, std::uint32_t v) {
        file[off+0] = v & 0xFF; file[off+1] = (v>>8) & 0xFF;
        file[off+2] = (v>>16) & 0xFF; file[off+3] = (v>>24) & 0xFF;
    };
    auto put_u16 = [&](std::size_t off, std::uint16_t v) {
        file[off+0] = v & 0xFF; file[off+1] = (v>>8) & 0xFF;
    };

    // Header
    std::memcpy(file.data(), "Advantage Table", 15);
    put_u32(24, NUM_RECS);   // rec_count
    put_u32(32, HDR_LEN);    // hdr_len
    put_u32(36, REC_LEN);    // rec_len

    // Field descriptor at offset 400: CHAR(4) "TAG"
    std::memcpy(file.data() + 400, "TAG", 3);
    put_u16(400 + 129, 4);   // type = CHAR
    put_u16(400 + 131, 5);   // record_offset (after 1+4 prefix bytes)
    put_u16(400 + 135, 4);   // length

    // Records
    auto add_rec = [&](std::uint8_t status, const char* s) {
        file.push_back(status);
        file.push_back(0); file.push_back(0);
        file.push_back(0); file.push_back(0);   // null bitmap
        for (int i = 0; i < 4; ++i)
            file.push_back(i < (int)std::strlen(s)
                           ? static_cast<std::uint8_t>(s[i]) : ' ');
    };
    add_rec(0x04, "AAAA");   // active
    add_rec(0x05, "BBBB");   // deleted
    add_rec(0x04, "CCCC");   // active
    add_rec(0x05, "DDDD");   // deleted

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

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
    hdr[10] = 1 + 4;
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
    rec(' ',  "AAAA");
    rec('*',  "BBBB");   // deleted
    rec(' ',  "CCCC");
    rec('*',  "DDDD");   // deleted
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("M9.8 AdsZapTable empties the DBF and indexes") {
    auto dir = fs::temp_directory_path() / "openads_m98_zap";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name[16] = "data";
    REQUIRE(AdsOpenTable(hConn, name, name, ADS_CDX, 1, 1, 0, 1, &hTable) == 0);

    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 4u);

    REQUIRE(AdsZapTable(hTable) == 0);

    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 0u);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.8 AdsPackTable removes deleted rows and renumbers recnos") {
    auto dir = fs::temp_directory_path() / "openads_m98_pack";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name[16] = "data";
    REQUIRE(AdsOpenTable(hConn, name, name, ADS_CDX, 1, 1, 0, 1, &hTable) == 0);

    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 4u);     // 4 rows total, 2 deleted

    REQUIRE(AdsPackTable(hTable) == 0);
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 2u);     // only the live rows survive

    // Surviving rows: AAAA at recno 1, CCCC at recno 2.
    REQUIRE(AdsGotoRecord(hTable, 1) == 0);
    UNSIGNED8 fld[16] = "TAG";
    UNSIGNED8 buf[16] = {0};
    UNSIGNED32 cap = sizeof(buf);
    REQUIRE(AdsGetField(hTable, fld, buf, &cap, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "AAAA");

    REQUIRE(AdsGotoRecord(hTable, 2) == 0);
    cap = sizeof(buf); std::memset(buf, 0, sizeof(buf));
    REQUIRE(AdsGetField(hTable, fld, buf, &cap, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "CCCC");

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsZapTable empties an ADT table and truncates the file") {
    auto dir = fs::temp_directory_path() / "openads_adt_zap";
    std::error_code ec;
    fs::remove_all(dir, ec);
    auto p = stage_adt(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name[] = "data";
    REQUIRE(AdsOpenTable(hConn, name, name,
                         ADS_ADT, ADS_ANSI, ADS_COMPATIBLE_LOCKING,
                         0, 0, &hTable) == 0);

    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 4u);

    REQUIRE(AdsZapTable(hTable) == 0);

    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 0u);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);

    // File must be truncated to exactly hdr_len (600 bytes) — no stale records.
    CHECK(fs::file_size(p) == 600u);

    fs::remove_all(dir, ec);
}

TEST_CASE("AdsPackTable removes deleted rows from an ADT table") {
    auto dir = fs::temp_directory_path() / "openads_adt_pack";
    std::error_code ec;
    fs::remove_all(dir, ec);
    auto p = stage_adt(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name[] = "data";
    REQUIRE(AdsOpenTable(hConn, name, name,
                         ADS_ADT, ADS_ANSI, ADS_COMPATIBLE_LOCKING,
                         0, 0, &hTable) == 0);

    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 4u);

    REQUIRE(AdsPackTable(hTable) == 0);
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == 0);
    CHECK(cnt == 2u);

    // Surviving rows: AAAA at recno 1, CCCC at recno 2.
    REQUIRE(AdsGotoRecord(hTable, 1) == 0);
    UNSIGNED8 fld[] = "TAG";
    UNSIGNED8 buf2[16] = {0};
    UNSIGNED32 cap2 = sizeof(buf2);
    REQUIRE(AdsGetField(hTable, fld, buf2, &cap2, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf2), 4) == "AAAA");

    REQUIRE(AdsGotoRecord(hTable, 2) == 0);
    cap2 = sizeof(buf2); std::memset(buf2, 0, sizeof(buf2));
    REQUIRE(AdsGetField(hTable, fld, buf2, &cap2, 0) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf2), 4) == "CCCC");

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);

    // File must be exactly hdr_len + 2*rec_len = 600 + 18 = 618 bytes.
    CHECK(fs::file_size(p) == 618u);

    fs::remove_all(dir, ec);
}
