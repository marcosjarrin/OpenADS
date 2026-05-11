// M12.23 root-cause fix: ACE auto-locks a freshly-appended record in a
// non-exclusive table, and AdsIsRecordLocked reports it. X#'s ADSRDD
// GoHot refuses to write a record it sees as unlocked, so without this
// the X# RDD errored "record is not locked" on the first FieldPut.
#include "doctest.h"
#include "openads/ace.h"
#include "openads/error.h"

#include <cstdint>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("M12.23 AdsAppendRecord auto-locks the new record; AdsIsRecordLocked sees it") {
    const auto dir = fs::temp_directory_path() / "openads_m1223_autolock";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER, nullptr, nullptr, 0, &hConn)
            == openads::AE_SUCCESS);

    UNSIGNED8 name[64]   = "lk";
    UNSIGNED8 alias[64]  = "lk";
    UNSIGNED8 fields[64] = "ID,Numeric,4,0;NAME,Character,8";
    ADSHANDLE hTable = 0;
    REQUIRE(AdsCreateTable(hConn, name, alias, ADS_CDX, 0, 0, 0, 64,
                           fields, &hTable) == openads::AE_SUCCESS);
    REQUIRE(AdsCloseTable(hTable) == openads::AE_SUCCESS);

    hTable = 0;
    REQUIRE(AdsOpenTable(hConn, name, alias, ADS_CDX, 0, 0, 0, 0, &hTable)
            == openads::AE_SUCCESS);

    // Nothing locked yet.
    UNSIGNED16 locked = 9;
    REQUIRE(AdsIsRecordLocked(hTable, 0, &locked) == openads::AE_SUCCESS);
    CHECK(locked == 0);

    REQUIRE(AdsAppendRecord(hTable) == openads::AE_SUCCESS);
    UNSIGNED32 rec = 0;
    REQUIRE(AdsGetRecordNum(hTable, 0, &rec) == openads::AE_SUCCESS);
    CHECK(rec == 1u);

    // Auto-locked: "current record" (0) and the explicit recno both report locked.
    locked = 9;
    REQUIRE(AdsIsRecordLocked(hTable, 0, &locked) == openads::AE_SUCCESS);
    CHECK(locked == 1);
    locked = 9;
    REQUIRE(AdsIsRecordLocked(hTable, rec, &locked) == openads::AE_SUCCESS);
    CHECK(locked == 1);
    locked = 9;
    REQUIRE(AdsIsRecordLocked(hTable, rec + 1, &locked) == openads::AE_SUCCESS);
    CHECK(locked == 0);

    // A write doesn't drop the lock; an explicit unlock does.
    UNSIGNED8 fID[8] = "ID";
    REQUIRE(AdsSetString(hTable, fID, (UNSIGNED8*)"1", 1) == openads::AE_SUCCESS);
    REQUIRE(AdsWriteRecord(hTable) == openads::AE_SUCCESS);
    locked = 9;
    REQUIRE(AdsIsRecordLocked(hTable, 0, &locked) == openads::AE_SUCCESS);
    CHECK(locked == 1);

    REQUIRE(AdsUnlockRecord(hTable, 0) == openads::AE_SUCCESS);
    locked = 9;
    REQUIRE(AdsIsRecordLocked(hTable, rec, &locked) == openads::AE_SUCCESS);
    CHECK(locked == 0);

    REQUIRE(AdsCloseTable(hTable) == openads::AE_SUCCESS);
    REQUIRE(AdsDisconnect(hConn) == openads::AE_SUCCESS);
    fs::remove_all(dir, ec);
}
