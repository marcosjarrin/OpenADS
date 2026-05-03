#include "engine/tx.h"

namespace openads::engine {

void Tx::note_before_image(TableId table, Recno recno,
                           std::vector<std::uint8_t> bytes) {
    if (!active_) return;
    RecordKey k{table, recno};
    if (appended_.find(k) != appended_.end()) return; // appended this tx
    before_images_.emplace(k, std::move(bytes));
}

void Tx::note_append(TableId table, Recno recno) {
    if (!active_) return;
    RecordKey k{table, recno};
    appended_[k] = true;
}

} // namespace openads::engine
