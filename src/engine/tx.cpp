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
    if (log_ != nullptr) {
        const auto& path = path_for(table);
        (void)log_->append_update(tx_id_, path, recno, before, after);
    }
}

void Tx::note_append(TableId table, Recno recno) {
    if (!active_) return;
    RecordKey k{table, recno};
    appended_[k] = true;
}

} // namespace openads::engine
