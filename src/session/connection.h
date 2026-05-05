#pragma once

#include "engine/data_dict.h"
#include "engine/lsn_map.h"
#include "engine/table.h"
#include "engine/tx.h"
#include "engine/tx_log.h"
#include "session/handle_registry.h"
#include "util/result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openads::session {

class Connection {
public:
    Connection() = default;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept = default;
    Connection& operator=(Connection&&) noexcept = default;

    static util::Result<Connection> open(const std::string& data_dir);

    util::Result<Handle>
        open_table(const std::string& relative_path,
                   engine::TableType  type,
                   engine::OpenMode   mode = engine::OpenMode::Shared,
                   engine::LockingMode locking = engine::LockingMode::Compatible);

    void close_table(Handle h);

    engine::Table* lookup_table(Handle h);

    const std::string& data_dir() const noexcept { return data_dir_; }

    // Transaction surface (M5).
    util::Result<void> begin_tx();
    util::Result<void> commit_tx();
    util::Result<void> rollback_tx();
    util::Result<void> create_savepoint(const std::string& name);
    util::Result<void> rollback_to_savepoint(const std::string& name);
    util::Result<void> release_savepoint(const std::string& name);
    bool               in_tx() const noexcept { return tx_.active(); }
    int                tx_nest_depth() const noexcept { return tx_nest_depth_; }

    // Data Dictionary surface (M6).
    bool has_dd() const noexcept { return dd_.has_value(); }
    engine::DataDict* dd() noexcept {
        return dd_.has_value() ? &*dd_ : nullptr;
    }

    // Table directory iteration (M9.12). Glob mask is matched against
    // each entry of `data_dir_` with `*` and `?` wildcards, case
    // insensitive on Windows. Returns AE_NO_FILE_FOUND if nothing
    // matches at the start; otherwise emits the first hit and a handle
    // the caller threads into find_next_table / find_close.
    struct TableFind {
        std::vector<std::string> matches;
        std::size_t              cursor = 0;
    };

    util::Result<std::pair<TableFind*, std::string>>
        find_first_table(const std::string& mask);
    util::Result<std::string> find_next_table(TableFind* find);
    util::Result<void>        find_close(TableFind* find);

private:
    util::Result<void> recover_orphan_tx_();

    std::string                                                data_dir_;
    std::unordered_map<Handle, std::unique_ptr<engine::Table>> tables_;
    std::unordered_map<Handle, std::string>                    table_paths_;
    Handle                                                     next_table_handle_ = 1;
    std::vector<std::unique_ptr<TableFind>>                    finds_;

    engine::TxLog                                              tx_log_;
    engine::LsnMap                                             lsn_map_;
    engine::Tx                                                 tx_;
    std::uint64_t                                              next_tx_id_ = 1;
    // M11.3 — nested BEGIN/COMMIT depth. Outer BEGIN sets it to 1;
    // each nested BEGIN bumps it; each nested COMMIT decrements;
    // only the outermost commit triggers real flush + log truncate.
    int                                                        tx_nest_depth_ = 0;

    std::optional<engine::DataDict>                            dd_;
};

} // namespace openads::session
