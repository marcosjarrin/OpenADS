#include "engine/table.h"

#include "drivers/cdx/cdx_driver.h"
#include "drivers/ntx/ntx_driver.h"

#include <utility>

namespace openads::engine {

util::Result<Table> Table::open(const std::string& path,
                                TableType type,
                                OpenMode mode,
                                LockingMode locking) {
    std::unique_ptr<drivers::IDriver> drv;
    switch (type) {
        case TableType::Cdx:
            drv = std::make_unique<drivers::cdx::CdxDriver>();
            break;
        case TableType::Ntx:
            drv = std::make_unique<drivers::ntx::NtxDriver>();
            break;
        case TableType::Adt:
        case TableType::Vfp:
            return util::Error{5004, 0,
                               "table type not yet supported in M2", path};
    }
    drivers::DriverOpenMode dmode = drivers::DriverOpenMode::ReadOnly;
    switch (mode) {
        case OpenMode::Read:      dmode = drivers::DriverOpenMode::ReadOnly;  break;
        case OpenMode::Shared:    dmode = drivers::DriverOpenMode::Shared;    break;
        case OpenMode::Exclusive: dmode = drivers::DriverOpenMode::Exclusive; break;
    }
    if (auto r = drv->open(path, dmode); !r) return r.error();
    return Table{std::move(drv), mode, locking, type};
}

std::uint16_t Table::field_count() const noexcept {
    return static_cast<std::uint16_t>(driver_->fields().size());
}

const drivers::DbfField& Table::field_descriptor(std::uint16_t idx) const {
    return driver_->fields().at(idx);
}

std::int32_t Table::field_index(const std::string& name) const noexcept {
    const auto& fs = driver_->fields();
    for (std::size_t i = 0; i < fs.size(); ++i) {
        if (fs[i].name == name) return static_cast<std::int32_t>(i);
    }
    return -1;
}

std::uint32_t Table::record_count() const noexcept {
    return driver_->record_count();
}

util::Result<void> Table::load_record_(std::uint32_t recno) {
    auto buf = driver_->read_record_raw(recno);
    if (!buf) return buf.error();
    record_buf_ = std::move(buf).value();
    recno_      = recno;
    state_      = State::Positioned;
    return {};
}

util::Result<void> Table::writeback_record_() {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    return driver_->write_record_raw(recno_, record_buf_.data(),
                                     record_buf_.size());
}

util::Result<void> Table::goto_top() {
    if (driver_->record_count() == 0) {
        state_ = State::Eof; recno_ = 0; return {};
    }
    return load_record_(1);
}

util::Result<void> Table::goto_bottom() {
    auto n = driver_->record_count();
    if (n == 0) { state_ = State::Eof; recno_ = 0; return {}; }
    return load_record_(n);
}

util::Result<void> Table::goto_record(std::uint32_t recno) {
    if (recno == 0 || recno > driver_->record_count()) {
        state_ = State::Eof; recno_ = 0;
        return util::Error{5000, 0, "recno out of range", ""};
    }
    return load_record_(recno);
}

util::Result<void> Table::skip(std::int32_t delta) {
    auto n = driver_->record_count();
    if (n == 0) { state_ = State::Eof; recno_ = 0; return {}; }
    std::int64_t target = static_cast<std::int64_t>(recno_) + delta;
    if (state_ == State::Bof && delta > 0) target = delta;
    if (target < 1) { state_ = State::Bof; recno_ = 0; return {}; }
    if (target > static_cast<std::int64_t>(n)) {
        state_ = State::Eof; recno_ = n + 1; return {};
    }
    return load_record_(static_cast<std::uint32_t>(target));
}

util::Result<drivers::DbfFieldValue>
Table::read_field(std::uint16_t field_index) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "table not positioned on a record", ""};
    }
    if (field_index >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    return drivers::decode_field(driver_->fields().at(field_index),
                                 record_buf_.data(), record_buf_.size());
}

util::Result<void> Table::append_record() {
    if (mode_ == OpenMode::Read) {
        return util::Error{5000, 0, "table opened read-only", ""};
    }
    auto rec = drivers::make_empty_record(driver_->record_length());
    auto new_recno = driver_->append_record_raw(rec.data(), rec.size());
    if (!new_recno) return new_recno.error();
    record_buf_ = std::move(rec);
    recno_      = new_recno.value();
    state_      = State::Positioned;
    return {};
}

util::Result<void> Table::set_field(std::uint16_t idx, const std::string& v) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    if (idx >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    auto r = drivers::encode_field_string(driver_->fields().at(idx),
                                          record_buf_.data(),
                                          record_buf_.size(), v);
    if (!r) return r.error();
    return writeback_record_();
}

util::Result<void> Table::set_field(std::uint16_t idx, double v) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    if (idx >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    auto r = drivers::encode_field_double(driver_->fields().at(idx),
                                          record_buf_.data(),
                                          record_buf_.size(), v);
    if (!r) return r.error();
    return writeback_record_();
}

util::Result<void> Table::set_field(std::uint16_t idx, bool v) {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    if (idx >= driver_->fields().size()) {
        return util::Error{5063, 0, "field index out of range", ""};
    }
    auto r = drivers::encode_field_logical(driver_->fields().at(idx),
                                           record_buf_.data(),
                                           record_buf_.size(), v);
    if (!r) return r.error();
    return writeback_record_();
}

util::Result<void> Table::mark_deleted() {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    drivers::set_record_deleted(record_buf_.data(), record_buf_.size(), true);
    return writeback_record_();
}

util::Result<void> Table::recall_deleted() {
    if (state_ != State::Positioned) {
        return util::Error{5000, 0, "no record positioned", ""};
    }
    drivers::set_record_deleted(record_buf_.data(), record_buf_.size(), false);
    return writeback_record_();
}

bool Table::is_deleted() const noexcept {
    if (state_ != State::Positioned) return false;
    return drivers::record_is_deleted(record_buf_.data(), record_buf_.size());
}

util::Result<void> Table::flush() {
    return driver_->flush();
}

TableTypeForLock Table::to_lock_type_() const noexcept {
    switch (type_) {
        case TableType::Cdx: return TableTypeForLock::Cdx;
        case TableType::Ntx: return TableTypeForLock::Ntx;
        case TableType::Adt: return TableTypeForLock::Adt;
        case TableType::Vfp: return TableTypeForLock::Vfp;
    }
    return TableTypeForLock::Cdx;
}

util::Result<void> Table::lock_record_excl(std::uint32_t recno) {
    if (mode_ == OpenMode::Read) return {};
    auto h = locks_.lock_record_excl(driver_->file(), to_lock_type_(),
                                     locking_, recno);
    if (!h) return h.error();
    recno_locks_.emplace(recno, std::move(h).value());
    return {};
}

util::Result<void> Table::unlock_record(std::uint32_t recno) {
    auto it = recno_locks_.find(recno);
    if (it != recno_locks_.end()) {
        it->second.release();
        recno_locks_.erase(it);
    }
    return {};
}

util::Result<void> Table::lock_table_excl() {
    if (mode_ == OpenMode::Read) return {};
    auto h = locks_.lock_table_excl(driver_->file(), to_lock_type_(), locking_);
    if (!h) return h.error();
    table_lock_ = std::move(h).value();
    return {};
}

util::Result<void> Table::unlock_table() {
    if (table_lock_) {
        table_lock_->release();
        table_lock_.reset();
    }
    return {};
}

} // namespace openads::engine
