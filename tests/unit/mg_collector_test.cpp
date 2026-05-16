#include "doctest.h"

#include "mgmt/mg_collector.h"
#include "mgmt/mg_snapshot.h"
#include "mgmt/mg_stats.h"

#include <cstring>
#include <string>

using openads::mgmt::MgCollector;
using openads::mgmt::MgSnapshot;
using openads::mgmt::MgStats;

namespace {
std::string c_str_of(const UNSIGNED8* p, std::size_t cap) {
    std::size_t n = 0;
    while (n < cap && p[n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
}
}  // namespace

TEST_CASE("MgCollector::install_info reports the product string") {
    MgSnapshot snap;
    MgStats    stats;
    MgCollector c(snap, stats);

    ADS_MGMT_INSTALL_INFO info = c.install_info();
    CHECK(c_str_of(info.aucVersionStr, sizeof(info.aucVersionStr))
              .rfind("OpenADS", 0) == 0);
    // OpenADS is not serial-licensed: serial reports empty.
    CHECK(c_str_of(info.aucSerialNumber,
                   sizeof(info.aucSerialNumber)).empty());
}

TEST_CASE("MgCollector::activity_info maps counts and uptime") {
    MgSnapshot snap;
    snap.connections    = 3;
    snap.workareas      = 7;
    snap.tables         = 5;
    snap.indexes        = 2;
    snap.locks          = 1;
    snap.worker_threads = 4;
    snap.user_list.resize(3);   // 3 users

    MgStats stats;
    stats.max_connections.store(9);
    stats.operations.store(120);
    stats.logged_errors.store(2);

    MgCollector c(snap, stats);
    ADS_MGMT_ACTIVITY_INFO a = c.activity_info();

    CHECK(a.ulOperations              == 120);
    CHECK(a.ulLoggedErrors            == 2);
    CHECK(a.stConnections.ulInUse     == 3);
    CHECK(a.stConnections.ulMaxUsed   == 9);
    CHECK(a.stWorkAreas.ulInUse       == 7);
    CHECK(a.stTables.ulInUse          == 5);
    CHECK(a.stIndexes.ulInUse         == 2);
    CHECK(a.stLocks.ulInUse           == 1);
    CHECK(a.stWorkerThreads.ulInUse   == 4);
    CHECK(a.stUsers.ulInUse           == 3);
}
