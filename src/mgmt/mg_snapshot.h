#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace openads::mgmt {

// One connected user / session.
struct MgUser {
    std::string                           name;
    std::string                           address;     // "ip:port"
    std::string                           os_login;
    std::uint16_t                         conn_no = 0;
    std::chrono::system_clock::time_point  connected_at{};
};

// One open table in a session.
struct MgTable {
    std::string   name;
    std::string   user;
    std::uint16_t conn_no   = 0;
    std::uint16_t open_mode = 0;   // 0 = shared, 1 = exclusive
    std::uint16_t lock_type = 0;   // ADS_MGMT_* lock constant
};

// One open index/order.
struct MgIndex {
    std::string name;
    std::string tag;
    std::string expression;
};

// One held lock.
struct MgLock {
    std::string   user;
    std::uint16_t conn_no = 0;
    std::uint32_t recno   = 0;
};

// One worker thread.
struct MgThread {
    std::uint32_t thread_no = 0;
    std::uint16_t opcode    = 0;
    std::string   user;
    std::uint16_t conn_no   = 0;
    std::string   os_login;
};

// Point-in-time raw telemetry. Built by collect_local_snapshot() in a
// DLL process or collect_server_snapshot(Server&) in the daemon, then
// formatted into ADS_MGMT_* structs by MgCollector.
struct MgSnapshot {
    std::uint32_t users           = 0;
    std::uint32_t connections     = 0;
    std::uint32_t workareas       = 0;
    std::uint32_t tables          = 0;
    std::uint32_t indexes         = 0;
    std::uint32_t locks           = 0;
    std::uint32_t worker_threads  = 0;
    std::uint16_t server_type     = 0;   // 0 = unknown/local

    std::vector<MgUser>   user_list;
    std::vector<MgTable>  table_list;
    std::vector<MgIndex>  index_list;
    std::vector<MgLock>   lock_list;
    std::vector<MgThread> thread_list;
};

}  // namespace openads::mgmt
