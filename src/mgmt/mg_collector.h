#pragma once

#include "mgmt/mg_snapshot.h"
#include "mgmt/mg_stats.h"
#include "openads/ace.h"

#include <vector>

namespace openads::mgmt {

// Formats a raw MgSnapshot + MgStats into the SAP-canonical
// ADS_MGMT_* structs declared in include/openads/ace.h. Pure: holds
// copies of its inputs and never touches global state, so it is
// trivially unit-testable with a fabricated snapshot.
class MgCollector {
public:
    MgCollector(MgSnapshot snapshot, const MgStats& stats);

    ADS_MGMT_INSTALL_INFO   install_info() const;
    ADS_MGMT_ACTIVITY_INFO  activity_info() const;
    ADS_MGMT_COMM_STATS     comm_stats() const;
    ADS_MGMT_CONFIG_PARAMS  config_params() const;
    ADS_MGMT_CONFIG_MEMORY  config_memory() const;

    std::vector<ADS_MGMT_USER_INFO>       user_names() const;
    std::vector<ADS_MGMT_TABLE_INFO>      open_tables() const;
    std::vector<ADS_MGMT_INDEX_INFO>      open_indexes() const;
    std::vector<ADS_MGMT_LOCK_INFO>       locks() const;
    std::vector<ADS_MGMT_THREAD_ACTIVITY> worker_thread_activity() const;

    // Returns the lock held on (conn-agnostic) `recno`; usConnNumber
    // is 0 and ulRecordNumber is 0 when no such lock exists.
    ADS_MGMT_LOCK_INFO lock_owner(std::uint32_t recno) const;

    std::uint16_t server_type() const { return snapshot_.server_type; }

    const MgSnapshot& snapshot() const { return snapshot_; }

private:
    MgSnapshot    snapshot_;
    // Plain copies of the counters captured at construction time.
    std::uint64_t packets_in_;
    std::uint64_t packets_out_;
    std::uint64_t bytes_in_;
    std::uint64_t bytes_out_;
    std::uint64_t disconnects_;
    std::uint64_t partial_connects_;
    std::uint64_t operations_;
    std::uint64_t logged_errors_;
    std::uint32_t max_users_;
    std::uint32_t max_connections_;
    std::uint32_t max_workareas_;
    std::uint32_t max_tables_;
    std::uint32_t max_indexes_;
    std::uint32_t max_locks_;
    long long     uptime_seconds_;
};

}  // namespace openads::mgmt
