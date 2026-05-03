#pragma once

#include "drivers/memo_trait.h"

#include <string>

namespace openads::drivers::dbt {

// dBase III memo store. 512-byte blocks. Header records the next free
// block in bytes 0-3 (uint32 LE). Each memo is terminated by 0x1A 0x1A.
class DbtMemo final : public IMemoStore {
public:
    util::Result<void>
        open(const std::string& path, MemoOpenMode mode) override;

    util::Result<std::string>
        read(std::uint32_t block_no) override;

    util::Result<std::uint32_t>
        write(const std::string& payload) override;

    util::Result<void> free_block(std::uint32_t block_no) override;
    util::Result<void> flush() override;

    std::uint16_t block_size() const override { return 512; }

    // Build a fresh empty .dbt on disk.
    static util::Result<DbtMemo>
        create(const std::string& path);

private:
    util::Result<void> rewrite_header_();

    platform::File  file_;
    MemoOpenMode    mode_       = MemoOpenMode::ReadOnly;
    std::uint32_t   next_avail_ = 1;  // block 0 is the header
};

} // namespace openads::drivers::dbt
