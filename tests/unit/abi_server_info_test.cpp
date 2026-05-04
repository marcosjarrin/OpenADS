#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"
#include "platform/time.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <regex>
#include <string>

namespace fs = std::filesystem;

TEST_CASE("M9.15 AdsGetServerName reports local host name") {
    const auto dir = fs::temp_directory_path() / "openads_m9_15_srv";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    UNSIGNED8 buf[256] = {0};
    UNSIGNED16 cap = sizeof(buf);
    REQUIRE(AdsGetServerName(hConn, buf, &cap) == 0);
    auto host = openads::platform::host_name();
    CHECK(cap == static_cast<UNSIGNED16>(host.size()));
    CHECK(std::string(reinterpret_cast<const char*>(buf), cap) == host);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.15 AdsGetServerName truncates into small buffer + reports full size") {
    const auto dir = fs::temp_directory_path() / "openads_m9_15_srv_trunc";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    auto host = openads::platform::host_name();
    if (host.size() < 2) {
        REQUIRE(AdsDisconnect(hConn) == 0);
        return;
    }
    UNSIGNED8 small[2] = {0, 0};
    UNSIGNED16 cap = sizeof(small);
    REQUIRE(AdsGetServerName(hConn, small, &cap) == 0);
    // *cap reports the full host name length, not the bytes written.
    CHECK(cap == static_cast<UNSIGNED16>(host.size()));
    CHECK(small[0] == static_cast<UNSIGNED8>(host[0]));

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.15 AdsGetServerTime returns ISO-shaped date + HH:MM:SS time + ms-of-day") {
    const auto dir = fs::temp_directory_path() / "openads_m9_15_time";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    UNSIGNED8 dbuf[16] = {0};
    UNSIGNED16 dlen = sizeof(dbuf);
    UNSIGNED8 tbuf[16] = {0};
    UNSIGNED16 tlen = sizeof(tbuf);
    SIGNED32 ms = -1;
    REQUIRE(AdsGetServerTime(hConn, dbuf, &dlen, &ms, tbuf, &tlen) == 0);

    CHECK(dlen == 10);
    CHECK(tlen == 8);
    std::regex date_rx(R"(^\d{4}-\d{2}-\d{2}$)");
    std::regex time_rx(R"(^\d{2}:\d{2}:\d{2}$)");
    CHECK(std::regex_match(std::string(reinterpret_cast<const char*>(dbuf), dlen),
                           date_rx));
    CHECK(std::regex_match(std::string(reinterpret_cast<const char*>(tbuf), tlen),
                           time_rx));
    CHECK(ms >= 0);
    CHECK(ms < 24 * 3600 * 1000);

    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
