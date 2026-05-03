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
    hdr[4]  = 1;
    hdr[8]  = 32 + 32 + 1; hdr[9]  = 0;
    hdr[10] = 1 + 5;       hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C'; fd[16] = 5;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(' ');
    file.push_back('H'); file.push_back('E'); file.push_back('L');
    file.push_back('L'); file.push_back('O');
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("M5.x WAL: crash mid-tx, reopen recovers via persistent WAL replay") {
    const auto dir = fs::temp_directory_path() / "openads_m5x_crash";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir, "data.dbf");

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    UNSIGNED8 leaf[64] = "data.dbf";

    // First connection: begin tx, update, then disconnect WITHOUT
    // commit or rollback — simulating a crash. The WAL captures the
    // UPDATE record so the next open can recover.
    {
        ADSHANDLE hConn = 0;
        REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                             nullptr, nullptr, 0, &hConn) == 0);
        ADSHANDLE hTable = 0;
        REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX,
                             0, 0, 0, 0, &hTable) == 0);
        REQUIRE(AdsBeginTransaction(hConn) == 0);
        REQUIRE(AdsGotoTop(hTable) == 0);
        UNSIGNED8 fld[16]  = "TAG";
        UNSIGNED8 nval[16] = "WORLD";
        REQUIRE(AdsSetString(hTable, fld, nval, 5) == 0);
        // Force flush so the update lands on disk before the "crash".
        REQUIRE(AdsFlushFileBuffers(hTable) == 0);

        // Confirm the on-disk content is now WORLD before the crash.
        REQUIRE(AdsCloseTable(hTable) == 0);
        REQUIRE(AdsDisconnect(hConn) == 0);  // simulated crash
    }

    // Verify pre-recovery: reading the file directly shows "WORLD".
    {
        std::ifstream f((dir / "data.dbf").string(), std::ios::binary);
        f.seekg(32 + 32 + 1 + 1);   // skip header + field desc + 0x0D + del byte
        std::string buf(5, '\0');
        f.read(buf.data(), 5);
        CHECK(buf == "WORLD");
    }

    // Second connection: AdsConnect60 runs WAL recovery and rolls the
    // orphan UPDATE back to "HELLO" because the tx never committed.
    {
        ADSHANDLE hConn = 0;
        REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                             nullptr, nullptr, 0, &hConn) == 0);
        ADSHANDLE hTable = 0;
        REQUIRE(AdsOpenTable(hConn, leaf, nullptr, ADS_CDX,
                             0, 0, 0, 0, &hTable) == 0);
        REQUIRE(AdsGotoTop(hTable) == 0);
        UNSIGNED8 fld[16]  = "TAG";
        UNSIGNED8 buf[16]  = {0};
        UNSIGNED32 cap = sizeof(buf);
        REQUIRE(AdsGetField(hTable, fld, buf, &cap, 0) == 0);
        CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == "HELLO");
        REQUIRE(AdsCloseTable(hTable) == 0);
        REQUIRE(AdsDisconnect(hConn) == 0);
    }

    fs::remove_all(dir, ec);
}
