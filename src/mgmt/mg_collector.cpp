#include "mgmt/mg_collector.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace openads::mgmt {

namespace {

// Copy `s` into a fixed UNSIGNED8[cap] field, NUL-terminated and
// truncated. Trailing bytes are zeroed so the struct has no
// uninitialised tail.
void put_field(UNSIGNED8* dst, std::size_t cap, const std::string& s) {
    std::memset(dst, 0, cap);
    if (cap == 0) return;
    std::size_t n = std::min(s.size(), cap - 1);
    std::memcpy(dst, s.data(), n);
}

}  // namespace

MgCollector::MgCollector(MgSnapshot snapshot, const MgStats& stats)
    : snapshot_(std::move(snapshot)),
      packets_in_(stats.packets_in.load()),
      packets_out_(stats.packets_out.load()),
      bytes_in_(stats.bytes_in.load()),
      bytes_out_(stats.bytes_out.load()),
      disconnects_(stats.disconnects.load()),
      partial_connects_(stats.partial_connects.load()),
      operations_(stats.operations.load()),
      logged_errors_(stats.logged_errors.load()),
      max_users_(stats.max_users.load()),
      max_connections_(stats.max_connections.load()),
      max_workareas_(stats.max_workareas.load()),
      max_tables_(stats.max_tables.load()),
      max_indexes_(stats.max_indexes.load()),
      max_locks_(stats.max_locks.load()) {
    auto now  = std::chrono::system_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                    now - stats.start_time).count();
    uptime_seconds_ = secs < 0 ? 0 : secs;
}

ADS_MGMT_INSTALL_INFO MgCollector::install_info() const {
    ADS_MGMT_INSTALL_INFO info;
    std::memset(&info, 0, sizeof(info));
    info.ulUserOption = 0;
    put_field(info.aucRegisteredOwner, sizeof(info.aucRegisteredOwner),
              "OpenADS");
    put_field(info.aucVersionStr, sizeof(info.aucVersionStr),
              "OpenADS 1.0");
    // aucSerialNumber / aucEvalExpireDate intentionally left empty:
    // OpenADS is not serial-licensed.
    return info;
}

ADS_MGMT_ACTIVITY_INFO MgCollector::activity_info() const {
    ADS_MGMT_ACTIVITY_INFO a;
    std::memset(&a, 0, sizeof(a));

    a.ulOperations   = static_cast<UNSIGNED32>(operations_);
    a.ulLoggedErrors = static_cast<UNSIGNED32>(logged_errors_);

    long long up = uptime_seconds_;
    a.stUpTime.usDays    = static_cast<UNSIGNED16>(up / 86400);
    a.stUpTime.usHours   = static_cast<UNSIGNED16>((up % 86400) / 3600);
    a.stUpTime.usMinutes = static_cast<UNSIGNED16>((up % 3600) / 60);
    a.stUpTime.usSeconds = static_cast<UNSIGNED16>(up % 60);

    auto usage = [](UNSIGNED32 in_use, UNSIGNED32 max_used) {
        ADS_MGMT_USAGE_STRUCT u;
        u.ulInUse   = in_use;
        u.ulMaxUsed = max_used < in_use ? in_use : max_used;
        u.ulRejected = 0;
        return u;
    };

    UNSIGNED32 nusers = static_cast<UNSIGNED32>(snapshot_.user_list.size());
    a.stUsers        = usage(nusers, max_users_);
    a.stConnections  = usage(snapshot_.connections, max_connections_);
    a.stWorkAreas    = usage(snapshot_.workareas, max_workareas_);
    a.stTables       = usage(snapshot_.tables, max_tables_);
    a.stIndexes      = usage(snapshot_.indexes, max_indexes_);
    a.stLocks        = usage(snapshot_.locks, max_locks_);
    a.stWorkerThreads = usage(snapshot_.worker_threads, 0);
    // TPS* elem usage left zero — transaction-processing internals are
    // not exposed.
    return a;
}

}  // namespace openads::mgmt
