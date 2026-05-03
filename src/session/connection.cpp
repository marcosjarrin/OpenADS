#include "session/connection.h"

#include "drivers/dbt/dbt_memo.h"
#include "drivers/fpt/fpt_memo.h"
#include "platform/path.h"

#include <filesystem>
#include <memory>
#include <utility>

namespace openads::session {

util::Result<Connection> Connection::open(const std::string& data_dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(data_dir, ec)) {
        return util::Error{5103, 0, "data directory not found", data_dir};
    }
    Connection c;
    c.data_dir_ = data_dir;
    return c;
}

util::Result<Handle> Connection::open_table(const std::string& relative_path,
                                            engine::TableType  type,
                                            engine::OpenMode   mode,
                                            engine::LockingMode locking) {
    namespace fs = std::filesystem;
    fs::path full = fs::path(data_dir_) / relative_path;
    auto resolved = platform::resolve_case_insensitive(full.string());

    auto t = engine::Table::open(resolved, type, mode, locking);
    if (!t) return t.error();

    auto holder = std::make_unique<engine::Table>(std::move(t).value());

    // Auto-attach a memo store if the table has M-fields and a sibling
    // memo file exists. CDX/VFP tables look for .fpt; NTX (Clipper)
    // looks for .dbt.
    bool has_memo_field = false;
    for (std::uint16_t i = 0; i < holder->field_count(); ++i) {
        if (holder->field_descriptor(i).type ==
            openads::drivers::DbfFieldType::Memo) {
            has_memo_field = true;
            break;
        }
    }
    if (has_memo_field) {
        fs::path stem = fs::path(resolved);
        auto memo_open_mode =
            (mode == engine::OpenMode::Read) ? openads::drivers::MemoOpenMode::ReadOnly
                                             : openads::drivers::MemoOpenMode::Shared;
        if (type == engine::TableType::Ntx) {
            stem.replace_extension(".dbt");
            std::error_code ec;
            if (fs::exists(stem, ec)) {
                auto m = std::make_unique<openads::drivers::dbt::DbtMemo>();
                if (m->open(stem.string(), memo_open_mode)) {
                    holder->attach_memo(std::move(m));
                }
            }
        } else {
            stem.replace_extension(".fpt");
            std::error_code ec;
            if (fs::exists(stem, ec)) {
                auto m = std::make_unique<openads::drivers::fpt::FptMemo>();
                if (m->open(stem.string(), memo_open_mode)) {
                    holder->attach_memo(std::move(m));
                }
            }
        }
    }

    Handle h = next_table_handle_++;
    if (tx_.active()) {
        holder->attach_tx(&tx_, static_cast<engine::Tx::TableId>(h));
    }
    tables_.emplace(h, std::move(holder));
    return h;
}

util::Result<void> Connection::begin_tx() {
    if (tx_.active()) {
        return util::Error{5000, 0, "transaction already active", ""};
    }
    tx_.activate(next_tx_id_++);
    for (auto& [h, holder] : tables_) {
        holder->attach_tx(&tx_, static_cast<engine::Tx::TableId>(h));
    }
    return {};
}

util::Result<void> Connection::commit_tx() {
    if (!tx_.active()) {
        return util::Error{5000, 0, "no active transaction", ""};
    }
    for (auto& [h, holder] : tables_) {
        (void)h;
        holder->detach_tx();
        // Persist driver buffers; without a WAL, durability ends at flush.
        (void)holder->flush();
    }
    tx_.clear();
    return {};
}

util::Result<void> Connection::rollback_tx() {
    if (!tx_.active()) {
        return util::Error{5000, 0, "no active transaction", ""};
    }
    // Restore before-images.
    tx_.for_each_before_image(
        [&](const engine::Tx::RecordKey& k,
            const std::vector<std::uint8_t>& bytes) {
            auto it = tables_.find(static_cast<Handle>(k.table));
            if (it == tables_.end()) return;
            auto* drv = it->second->driver();
            if (drv) {
                (void)drv->write_record_raw(k.recno, bytes.data(), bytes.size());
            }
        });
    // Mark records appended in this tx as deleted.
    tx_.for_each_append([&](const engine::Tx::RecordKey& k) {
        auto it = tables_.find(static_cast<Handle>(k.table));
        if (it == tables_.end()) return;
        auto* drv = it->second->driver();
        if (!drv) return;
        auto rec = drv->read_record_raw(k.recno);
        if (!rec) return;
        auto buf = std::move(rec).value();
        openads::drivers::set_record_deleted(buf.data(), buf.size(), true);
        (void)drv->write_record_raw(k.recno, buf.data(), buf.size());
    });
    for (auto& [h, holder] : tables_) {
        (void)h;
        holder->detach_tx();
        (void)holder->flush();
    }
    tx_.clear();
    return {};
}

void Connection::close_table(Handle h) {
    tables_.erase(h);
}

engine::Table* Connection::lookup_table(Handle h) {
    auto it = tables_.find(h);
    if (it == tables_.end()) return nullptr;
    return it->second.get();
}

} // namespace openads::session
