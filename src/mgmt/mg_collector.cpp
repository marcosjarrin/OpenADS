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

ADS_MGMT_COMM_STATS MgCollector::comm_stats() const {
    ADS_MGMT_COMM_STATS s;
    std::memset(&s, 0, sizeof(s));
    s.ulTotalPackets      = static_cast<UNSIGNED32>(
        packets_in_ + packets_out_);
    s.ulDisconnectedUsers = static_cast<UNSIGNED32>(disconnects_);
    s.ulPartialConnects   = static_cast<UNSIGNED32>(partial_connects_);
    // dPercentCheckSums, ulCheckSumFailures, ulRcvPktOutOfSeq,
    // ulRcvReqOutOfSeq, ulNotLoggedIn, ulInvalidPackets,
    // ulRecvFromErrors, ulSendToErrors — no analogue in OpenADS'
    // TCP framing; left as honest zeros.
    return s;
}

ADS_MGMT_CONFIG_PARAMS MgCollector::config_params() const {
    ADS_MGMT_CONFIG_PARAMS p;
    std::memset(&p, 0, sizeof(p));
    p.ulNumConnections   = snapshot_.connections;
    p.ulNumWorkAreas     = snapshot_.workareas;
    p.ulNumTables        = snapshot_.tables;
    p.ulNumIndexes       = snapshot_.indexes;
    p.ulNumLocks         = snapshot_.locks;
    p.usNumWorkerThreads = static_cast<UNSIGNED16>(
        snapshot_.worker_threads);
    // ECB / burst-packet / TPS fields left zero — NetWare-era, no
    // analogue. Path strings left empty.
    return p;
}

ADS_MGMT_CONFIG_MEMORY MgCollector::config_memory() const {
    ADS_MGMT_CONFIG_MEMORY m;
    std::memset(&m, 0, sizeof(m));
    // Per-category accounting is out of scope (no allocator
    // instrumentation). ulTotalConfigMem stays 0 here; a process-RSS
    // total can be wired in later without changing this interface.
    return m;
}

std::vector<ADS_MGMT_USER_INFO> MgCollector::user_names() const {
    std::vector<ADS_MGMT_USER_INFO> out;
    out.reserve(snapshot_.user_list.size());
    for (const auto& u : snapshot_.user_list) {
        ADS_MGMT_USER_INFO i;
        std::memset(&i, 0, sizeof(i));
        put_field(i.aucUserName, sizeof(i.aucUserName), u.name);
        put_field(i.aucAddress, sizeof(i.aucAddress), u.address);
        put_field(i.aucOSUserLoginName,
                  sizeof(i.aucOSUserLoginName), u.os_login);
        put_field(i.aucAuthUserName,
                  sizeof(i.aucAuthUserName), u.name);
        i.usConnNumber = u.conn_no;
        out.push_back(i);
    }
    return out;
}

std::vector<ADS_MGMT_TABLE_INFO> MgCollector::open_tables() const {
    std::vector<ADS_MGMT_TABLE_INFO> out;
    out.reserve(snapshot_.table_list.size());
    for (const auto& t : snapshot_.table_list) {
        ADS_MGMT_TABLE_INFO i;
        std::memset(&i, 0, sizeof(i));
        put_field(i.aucTableName, sizeof(i.aucTableName), t.name);
        put_field(i.aucUserName, sizeof(i.aucUserName), t.user);
        i.usConnNumber = t.conn_no;
        i.usOpenMode   = t.open_mode;
        i.usLockType   = t.lock_type;
        out.push_back(i);
    }
    return out;
}

std::vector<ADS_MGMT_INDEX_INFO> MgCollector::open_indexes() const {
    std::vector<ADS_MGMT_INDEX_INFO> out;
    out.reserve(snapshot_.index_list.size());
    for (const auto& x : snapshot_.index_list) {
        ADS_MGMT_INDEX_INFO i;
        std::memset(&i, 0, sizeof(i));
        put_field(i.aucIndexName, sizeof(i.aucIndexName), x.name);
        put_field(i.aucTagName, sizeof(i.aucTagName), x.tag);
        put_field(i.aucExpression, sizeof(i.aucExpression),
                  x.expression);
        out.push_back(i);
    }
    return out;
}

std::vector<ADS_MGMT_LOCK_INFO> MgCollector::locks() const {
    std::vector<ADS_MGMT_LOCK_INFO> out;
    out.reserve(snapshot_.lock_list.size());
    for (const auto& l : snapshot_.lock_list) {
        ADS_MGMT_LOCK_INFO i;
        std::memset(&i, 0, sizeof(i));
        put_field(i.aucUserName, sizeof(i.aucUserName), l.user);
        i.usConnNumber   = l.conn_no;
        i.ulRecordNumber = l.recno;
        out.push_back(i);
    }
    return out;
}

std::vector<ADS_MGMT_THREAD_ACTIVITY>
MgCollector::worker_thread_activity() const {
    std::vector<ADS_MGMT_THREAD_ACTIVITY> out;
    out.reserve(snapshot_.thread_list.size());
    for (const auto& t : snapshot_.thread_list) {
        ADS_MGMT_THREAD_ACTIVITY i;
        std::memset(&i, 0, sizeof(i));
        i.ulThreadNumber = t.thread_no;
        i.usOpCode       = t.opcode;
        i.usConnNumber   = t.conn_no;
        put_field(i.aucUserName, sizeof(i.aucUserName), t.user);
        put_field(i.aucOSUserLoginName,
                  sizeof(i.aucOSUserLoginName), t.os_login);
        out.push_back(i);
    }
    return out;
}

ADS_MGMT_LOCK_INFO MgCollector::lock_owner(std::uint32_t recno) const {
    ADS_MGMT_LOCK_INFO i;
    std::memset(&i, 0, sizeof(i));
    for (const auto& l : snapshot_.lock_list) {
        if (l.recno == recno) {
            put_field(i.aucUserName, sizeof(i.aucUserName), l.user);
            i.usConnNumber   = l.conn_no;
            i.ulRecordNumber = l.recno;
            break;
        }
    }
    return i;
}

// Process-global MgStats singleton. start_time is fixed the first
// time this is called; the server overwrites it at Server::start().
MgStats& process_mg_stats() {
    static MgStats g_stats;
    return g_stats;
}

}  // namespace openads::mgmt
