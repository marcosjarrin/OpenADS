#pragma once

#include "drivers/driver_trait.h"
#include "drivers/dbf_common.h"
#include "engine/lock_mgr.h"
#include "util/result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openads::engine {

enum class TableType { Cdx, Ntx, Adt, Vfp };
enum class OpenMode  { Read, Shared, Exclusive };

class Table {
public:
    Table() = default;
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;
    Table(Table&&) noexcept = default;
    Table& operator=(Table&&) noexcept = default;
    ~Table() = default;

    static util::Result<Table> open(const std::string& path,
                                    TableType type,
                                    OpenMode mode = OpenMode::Read,
                                    LockingMode locking = LockingMode::Compatible);

    std::uint16_t field_count() const noexcept;
    const drivers::DbfField& field_descriptor(std::uint16_t idx) const;
    std::int32_t field_index(const std::string& name) const noexcept;

    std::uint32_t record_count() const noexcept;
    std::uint32_t recno() const noexcept { return recno_; }
    bool eof() const noexcept { return state_ == State::Eof; }
    bool bof() const noexcept { return state_ == State::Bof; }

    util::Result<void> goto_top();
    util::Result<void> goto_bottom();
    util::Result<void> goto_record(std::uint32_t recno);
    util::Result<void> skip(std::int32_t delta);

    util::Result<drivers::DbfFieldValue>
        read_field(std::uint16_t field_index);

    // Write surface.
    util::Result<void> append_record();
    util::Result<void> set_field(std::uint16_t field_index,
                                 const std::string& value);
    util::Result<void> set_field(std::uint16_t field_index, double value);
    util::Result<void> set_field(std::uint16_t field_index, bool value);
    util::Result<void> mark_deleted();
    util::Result<void> recall_deleted();
    bool               is_deleted() const noexcept;
    util::Result<void> flush();

    // Locking surface.
    util::Result<void> lock_record_excl(std::uint32_t recno);
    util::Result<void> unlock_record    (std::uint32_t recno);
    util::Result<void> lock_table_excl();
    util::Result<void> unlock_table();

private:
    enum class State { Bof, Positioned, Eof };

    Table(std::unique_ptr<drivers::IDriver> drv,
          OpenMode mode, LockingMode locking, TableType type) noexcept
        : driver_(std::move(drv)), mode_(mode),
          locking_(locking), type_(type) {}

    util::Result<void> load_record_(std::uint32_t recno);
    util::Result<void> writeback_record_();

    TableTypeForLock to_lock_type_() const noexcept;

    std::unique_ptr<drivers::IDriver>             driver_;
    OpenMode                                      mode_     = OpenMode::Read;
    LockingMode                                   locking_  = LockingMode::Compatible;
    TableType                                     type_     = TableType::Cdx;
    LockMgr                                       locks_;
    std::unordered_map<std::uint32_t, LockHandle> recno_locks_;
    std::optional<LockHandle>                     table_lock_;
    State                                         state_  = State::Bof;
    std::uint32_t                                 recno_  = 0;
    std::vector<std::uint8_t>                     record_buf_;
};

} // namespace openads::engine
