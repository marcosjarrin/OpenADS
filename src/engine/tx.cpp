#include "engine/tx.h"

namespace openads::engine {

void Tx::note_before_image(TableId table, Recno recno,
                           std::vector<std::uint8_t> before,
                           std::vector<std::uint8_t> after) {
    if (!active_) return;
    RecordKey k{table, recno};
    bool first_touch = appended_.find(k) == appended_.end()
                       && before_images_.find(k) == before_images_.end();
    if (first_touch) {
        before_images_.emplace(k, before);
    }
    ops_.push_back(OrderedOp{table, recno, false, before});
    if (log_ != nullptr) {
        const auto& path = path_for(table);
        (void)log_->append_update(tx_id_, path, recno, before, after);
    }
}

void Tx::note_append(TableId table, Recno recno) {
    if (!active_) return;
    RecordKey k{table, recno};
    appended_[k] = true;
    ops_.push_back(OrderedOp{table, recno, true, {}});
}

void Tx::create_savepoint(const std::string& name) {
    if (!active_) return;
    savepoints_.emplace_back(name, ops_.size());
}

std::size_t Tx::savepoint_index(const std::string& name) const {
    for (auto it = savepoints_.rbegin(); it != savepoints_.rend(); ++it) {
        if (it->first == name) return it->second;
    }
    return static_cast<std::size_t>(-1);
}

void Tx::truncate_ops_to(std::size_t idx) {
    if (idx > ops_.size()) idx = ops_.size();
    ops_.resize(idx);
    while (!savepoints_.empty() && savepoints_.back().second > idx) {
        savepoints_.pop_back();
    }
}

bool Tx::release_savepoint(const std::string& name) {
    // M11.3 — drop the most recent savepoint with this name; ops are
    // kept (the changes between create + release stay part of the
    // outer transaction).
    for (auto it = savepoints_.rbegin(); it != savepoints_.rend(); ++it) {
        if (it->first == name) {
            savepoints_.erase(std::next(it).base());
            return true;
        }
    }
    return false;
}

} // namespace openads::engine
