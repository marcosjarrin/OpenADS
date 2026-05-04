#include "doctest.h"
#include "openads/ace.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

TEST_CASE("M9.5 AdsCreateTable: parse field defs + write DBF + open") {
    const auto dir = fs::temp_directory_path() / "openads_m95_create";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    UNSIGNED8 name[64]   = "newtable";
    UNSIGNED8 alias[64]  = "newtable";
    UNSIGNED8 fields[256] =
        "ID,Numeric,5,0;NAME,Character,12;ACTIVE,Logical;BORN,Date";

    ADSHANDLE hTable = 0;
    REQUIRE(AdsCreateTable(hConn, name, alias,
                           ADS_CDX, 0, 0, 0, 64,
                           fields, &hTable) == 0);

    UNSIGNED16 nflds = 0;
    REQUIRE(AdsGetNumFields(hTable, &nflds) == 0);
    CHECK(nflds == 4);

    UNSIGNED32 reclen = 0;
    REQUIRE(AdsGetRecordLength(hTable, &reclen) == 0);
    // 1 (delete) + 5 (ID) + 12 (NAME) + 1 (ACTIVE) + 8 (BORN) = 27
    CHECK(reclen == 27u);

    UNSIGNED16 type = 0;
    UNSIGNED8 fld[16] = "NAME";
    REQUIRE(AdsGetFieldType(hTable, fld, &type) == 0);
    CHECK(type == ADS_STRING);

    UNSIGNED32 flen = 0;
    REQUIRE(AdsGetFieldLength(hTable, fld, &flen) == 0);
    CHECK(flen == 12u);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
