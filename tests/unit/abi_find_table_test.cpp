#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

void make_dbf(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    auto p = dir / leaf;
    fs::remove(p);
    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0] = 0x03;
    hdr[4] = 0;
    hdr[8] = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 5;     hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C'; fd[16] = 5;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
}

void touch(const fs::path& dir, const char* leaf) {
    fs::create_directories(dir);
    std::ofstream(dir / leaf).put('x');
}

UNSIGNED32 connect_to(const fs::path& dir, ADSHANDLE& hConn) {
    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    return AdsConnect60(srv, ADS_LOCAL_SERVER,
                        nullptr, nullptr, 0, &hConn);
}

}  // namespace

TEST_CASE("AdsFindFirstTable + AdsFindNextTable + AdsFindClose iterate matching files") {
    const auto dir = fs::temp_directory_path() / "openads_m9_12_find";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir, "alpha.dbf");
    make_dbf(dir, "beta.dbf");
    make_dbf(dir, "gamma.dbf");
    touch(dir, "notes.txt");

    ADSHANDLE hConn = 0;
    REQUIRE(connect_to(dir, hConn) == 0);

    UNSIGNED8 mask[16] = "*.dbf";
    UNSIGNED8 name[256] = {0};
    UNSIGNED16 cap = sizeof(name);
    ADSHANDLE hFind = 0;

    REQUIRE(AdsFindFirstTable(hConn, mask, name, &cap, &hFind) == 0);
    std::unordered_set<std::string> found;
    found.insert(std::string(reinterpret_cast<const char*>(name), cap));

    for (;;) {
        std::memset(name, 0, sizeof(name));
        cap = sizeof(name);
        UNSIGNED32 rc = AdsFindNextTable(hConn, hFind, name, &cap);
        if (rc != 0) break;
        found.insert(std::string(reinterpret_cast<const char*>(name), cap));
    }
    REQUIRE(AdsFindClose(hConn, hFind) == 0);

    CHECK(found.size() == 3);
    CHECK(found.count("alpha.dbf") == 1);
    CHECK(found.count("beta.dbf")  == 1);
    CHECK(found.count("gamma.dbf") == 1);
    CHECK(found.count("notes.txt") == 0);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsFindFirstTable on empty mask = AE_NO_FILE_FOUND") {
    const auto dir = fs::temp_directory_path() / "openads_m9_12_empty";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    touch(dir, "readme.md");

    ADSHANDLE hConn = 0;
    REQUIRE(connect_to(dir, hConn) == 0);

    UNSIGNED8 mask[16] = "*.dbf";
    UNSIGNED8 name[256] = {0};
    UNSIGNED16 cap = sizeof(name);
    ADSHANDLE hFind = 0;

    UNSIGNED32 rc = AdsFindFirstTable(hConn, mask, name, &cap, &hFind);
    CHECK(rc == openads::AE_NO_FILE_FOUND);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsFindFirstTable with question-mark wildcard") {
    const auto dir = fs::temp_directory_path() / "openads_m9_12_qmark";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir, "a1.dbf");
    make_dbf(dir, "a2.dbf");
    make_dbf(dir, "long.dbf");

    ADSHANDLE hConn = 0;
    REQUIRE(connect_to(dir, hConn) == 0);

    UNSIGNED8 mask[16] = "a?.dbf";
    UNSIGNED8 name[256] = {0};
    UNSIGNED16 cap = sizeof(name);
    ADSHANDLE hFind = 0;

    REQUIRE(AdsFindFirstTable(hConn, mask, name, &cap, &hFind) == 0);
    std::unordered_set<std::string> found;
    found.insert(std::string(reinterpret_cast<const char*>(name), cap));

    for (;;) {
        std::memset(name, 0, sizeof(name));
        cap = sizeof(name);
        UNSIGNED32 rc = AdsFindNextTable(hConn, hFind, name, &cap);
        if (rc != 0) break;
        found.insert(std::string(reinterpret_cast<const char*>(name), cap));
    }
    REQUIRE(AdsFindClose(hConn, hFind) == 0);

    CHECK(found.size() == 2);
    CHECK(found.count("a1.dbf")   == 1);
    CHECK(found.count("a2.dbf")   == 1);
    CHECK(found.count("long.dbf") == 0);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsFindFirstTable case-insensitive match on Windows-style mask") {
    const auto dir = fs::temp_directory_path() / "openads_m9_12_case";
    std::error_code ec;
    fs::remove_all(dir, ec);
    make_dbf(dir, "MIXED.DBF");

    ADSHANDLE hConn = 0;
    REQUIRE(connect_to(dir, hConn) == 0);

    UNSIGNED8 mask[16] = "*.dbf";
    UNSIGNED8 name[256] = {0};
    UNSIGNED16 cap = sizeof(name);
    ADSHANDLE hFind = 0;

    REQUIRE(AdsFindFirstTable(hConn, mask, name, &cap, &hFind) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(name), cap) == "MIXED.DBF");
    REQUIRE(AdsFindClose(hConn, hFind) == 0);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("AdsFindClose with bogus handle = AE_INTERNAL_ERROR-ish") {
    const auto dir = fs::temp_directory_path() / "openads_m9_12_bogus";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    ADSHANDLE hConn = 0;
    REQUIRE(connect_to(dir, hConn) == 0);

    UNSIGNED32 rc = AdsFindClose(hConn, /*hFind=*/9999);
    CHECK(rc != 0);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
