// M12.22 — versioned ACE overloads exercised over the wire against a
// live openads_serverd. Gated on OPENADS_TEST_REMOTE: set it to the
// server URI (e.g. "tcp://host:16262/") whose data dir holds
// customer.dbf, otherwise this case is skipped.
#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
const char* remote_uri_env() { return std::getenv("OPENADS_TEST_REMOTE"); }
}  // namespace

TEST_CASE("M12.22 versioned overloads over the wire"
          * doctest::skip(remote_uri_env() == nullptr)) {
    const std::string uri = remote_uri_env();
    std::array<UNSIGNED8, 512> srv{};
    REQUIRE(uri.size() < srv.size());
    std::memcpy(srv.data(), uri.c_str(), uri.size() + 1);

    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect26(srv.data(), ADS_REMOTE_SERVER, &hConn)
            == openads::AE_SUCCESS);
    REQUIRE(hConn != 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 tname[64] = "customer.dbf";
    UNSIGNED8 alias[64] = "cust";
    UNSIGNED8 coll[8]   = "";
    REQUIRE(AdsOpenTable90(hConn, tname, alias, ADS_CDX, 0, 0, 0, 0,
                           coll, &hTable) == openads::AE_SUCCESS);
    REQUIRE(hTable != 0);

    UNSIGNED32 cnt = 0;
    REQUIRE(AdsGetRecordCount(hTable, 0, &cnt) == openads::AE_SUCCESS);
    CHECK(cnt > 0u);

    SUBCASE("AdsGetBookmark60 / AdsGotoBookmark60 roundtrip over the wire") {
        REQUIRE(AdsGotoTop(hTable) == openads::AE_SUCCESS);
        UNSIGNED32 rtop = 0;
        REQUIRE(AdsGetRecordNum(hTable, 0, &rtop) == openads::AE_SUCCESS);

        std::array<UNSIGNED8, 16> bm{};
        UNSIGNED32 bml = static_cast<UNSIGNED32>(bm.size());
        REQUIRE(AdsGetBookmark60(hTable, bm.data(), &bml) == openads::AE_SUCCESS);
        CHECK(bml == 4u);

        REQUIRE(AdsGotoBottom(hTable) == openads::AE_SUCCESS);
        UNSIGNED32 rbot = 0;
        REQUIRE(AdsGetRecordNum(hTable, 0, &rbot) == openads::AE_SUCCESS);
        if (cnt > 1u) CHECK(rbot != rtop);

        REQUIRE(AdsGotoBookmark60(hTable, bm.data()) == openads::AE_SUCCESS);
        UNSIGNED32 rback = 0;
        REQUIRE(AdsGetRecordNum(hTable, 0, &rback) == openads::AE_SUCCESS);
        CHECK(rback == rtop);
    }

    SUBCASE("forward shims return success against a remote handle") {
        UNSIGNED16 ex = 9;
        CHECK(AdsGetExact22(hTable, &ex) == openads::AE_SUCCESS);
        UNSIGNED8 fmt[16] = "";
        UNSIGNED16 fmtlen = sizeof(fmt);
        CHECK(AdsGetDateFormat60(hConn, fmt, &fmtlen) == openads::AE_SUCCESS);
        CHECK(AdsCancelUpdate90(hTable, 0) == openads::AE_SUCCESS);
        UNSIGNED64 v = 0;
        CHECK(AdsSetProperty90(hTable, 1, &v) == openads::AE_SUCCESS);
    }

    REQUIRE(AdsCloseTable(hTable) == openads::AE_SUCCESS);
    REQUIRE(AdsDisconnect(hConn) == openads::AE_SUCCESS);
}
