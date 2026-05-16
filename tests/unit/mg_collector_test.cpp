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

TEST_CASE("MgCollector::comm_stats reports real packet totals only") {
    MgSnapshot snap;
    MgStats    stats;
    stats.packets_in.store(40);
    stats.packets_out.store(60);
    stats.disconnects.store(2);
    stats.partial_connects.store(1);

    MgCollector c(snap, stats);
    ADS_MGMT_COMM_STATS s = c.comm_stats();

    CHECK(s.ulTotalPackets      == 100);   // in + out
    CHECK(s.ulDisconnectedUsers == 2);
    CHECK(s.ulPartialConnects   == 1);
    // No checksum / sequencing in our framing — honest zeros.
    CHECK(s.dPercentCheckSums   == doctest::Approx(0.0));
    CHECK(s.ulCheckSumFailures  == 0);
    CHECK(s.ulRcvPktOutOfSeq    == 0);
}

TEST_CASE("MgCollector::config_params echoes live counts") {
    MgSnapshot snap;
    snap.connections = 3;
    snap.tables      = 5;
    snap.worker_threads = 4;
    MgStats stats;

    MgCollector c(snap, stats);
    ADS_MGMT_CONFIG_PARAMS p = c.config_params();
    CHECK(p.ulNumConnections  == 3);
    CHECK(p.ulNumTables       == 5);
    CHECK(p.usNumWorkerThreads == 4);
    // NetWare-era ECB fields are honest zeros.
    CHECK(p.usNumReceiveECBs  == 0);
    CHECK(p.usNumSendECBs     == 0);
}
