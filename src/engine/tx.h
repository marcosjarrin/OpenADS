#pragma once

#include "engine/tx_log.h"
#include "util/result.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openads::engine {

// In-memory transaction state. Each Connection holds at most one Tx.
// During a transaction, every Table::write_record_raw call records a
// before-image keyed by (table_id, recno). On commit() the before-
// images are dropped; on rollback() they are written back to disk.
//
// Persistent WAL is wired through `log_`: when set, every
// note_before_image() also appends an UPDATE record so a crash before
// commit/abort can be recovered on the next Connection::open.

class Tx {
public:
    using TableId = std::uint32_t;
    using Recno   = std::uint32_t;

    struct RecordKey {
        TableId table;
        Recno   recno;
        bool operator==(const RecordKey& o) const noexcept {
            return table == o.table && recno == o.recno;
        }
    };
    struct RecordKeyHash {
        std::size_t operator()(const RecordKey& k) const noexcept {
            return std::hash<std::uint64_t>{}(
                (static_cast<std::uint64_t>(k.table) << 32) | k.recno);
        }
    };

    Tx() = default;

    void note_before_image(TableId table, Recno recno,
                           std::vector<std::uint8_t> before,
                           std::vector<std::uint8_t> after);

    void note_append(TableId table, Recno recno);

    // Savepoints (in-memory, M5.3).
    struct OrderedOp {
        TableId                  table;
        Recno                    recno;
        bool                     is_append;
        std::vector<std::uint8_t> before;
    };

    void create_savepoint(const std::string& name);
    const std::vector<OrderedOp>& ops() const noexcept { return ops_; }
    std::size_t savepoint_index(const std::string& name) const;
    void truncate_ops_to(std::size_t idx);

    bool active() const noexcept { return active_; }

    void activate(std::uint64_t id, TxLog* log) noexcept {
        active_ = true; tx_id_ = id; log_ = log;
        before_images_.clear(); appended_.clear(); paths_.clear();
    }
    void clear() noexcept {
        active_ = false; log_ = nullptr;
        before_images_.clear(); appended_.clear(); paths_.clear();
        ops_.clear(); savepoints_.clear();
    }

    void register_table(TableId id, std::string relative_path) {
        paths_[id] = std::move(relative_path);
    }
    const std::string& path_for(TableId id) const {
        static const std::string empty;
        auto it = paths_.find(id);
        return it == paths_.end() ? empty : it->second;
    }

    TxLog* log() noexcept { return log_; }

    std::uint64_t id() const noexcept { return tx_id_; }

    template <class F>
    void for_each_before_image(F&& f) const {
        for (const auto& [k, v] : before_images_) f(k, v);
    }
    template <class F>
    void for_each_append(F&& f) const {
        for (const auto& [k, _] : appended_) f(k);
    }

private:
    bool          active_  = false;
    std::uint64_t tx_id_   = 0;
    TxLog*        log_     = nullptr;

    std::unordered_map<RecordKey, std::vector<std::uint8_t>,
                       RecordKeyHash> before_images_;
    std::unordered_map<RecordKey, bool, RecordKeyHash> appended_;
    std::unordered_map<TableId, std::string>           paths_;

    std::vector<OrderedOp>                              ops_;
    std::vector<std::pair<std::string, std::size_t>>    savepoints_;
};

} // namespace openads::engine
