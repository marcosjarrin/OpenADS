#pragma once

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
// Persistent WAL with crash recovery is a follow-up (M5.x); this
// in-memory variant gives the ABI surface (AdsBeginTransaction /
// AdsCommitTransaction / AdsRollbackTransaction) usable semantics
// for single-process workflows.

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

    // Marks a record as touched and remembers its pre-write content.
    // No-op if a before-image for this record already exists.
    void note_before_image(TableId table, Recno recno,
                           std::vector<std::uint8_t> bytes);

    // Note an append: the record was created during the tx, so a
    // rollback must logically delete it (we mark it for deletion via
    // the record's deletion byte rather than physically truncating).
    void note_append(TableId table, Recno recno);

    bool active() const noexcept { return active_; }

    void activate(std::uint64_t id) noexcept {
        active_ = true; tx_id_ = id; before_images_.clear();
        appended_.clear();
    }
    void clear() noexcept {
        active_ = false; before_images_.clear(); appended_.clear();
    }

    std::uint64_t id() const noexcept { return tx_id_; }

    // Iterate before-images / appends for rollback.
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

    std::unordered_map<RecordKey, std::vector<std::uint8_t>,
                       RecordKeyHash> before_images_;
    std::unordered_map<RecordKey, bool, RecordKeyHash> appended_;
};

} // namespace openads::engine
