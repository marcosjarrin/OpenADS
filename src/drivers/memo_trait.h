#pragma once

#include "platform/file.h"
#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace openads::drivers {

enum class MemoOpenMode { ReadOnly, Shared, Exclusive };

// A MemoStore owns the secondary memo file (.dbt / .fpt / .adm).
// Drivers bind to one when the table opens; M-type fields decode their
// 10-byte ASCII block-number references into content via this interface.
class IMemoStore {
public:
    virtual ~IMemoStore() = default;

    virtual util::Result<void>
        open(const std::string& path, MemoOpenMode mode) = 0;

    // Read the memo at `block_no` (1-based; 0 means "no memo").
    virtual util::Result<std::string>
        read(std::uint32_t block_no) = 0;

    // Allocate and write a memo. Returns the assigned block number.
    virtual util::Result<std::uint32_t>
        write(const std::string& payload) = 0;

    // Mark a memo's blocks as free.
    virtual util::Result<void> free_block(std::uint32_t block_no) = 0;

    virtual util::Result<void> flush() = 0;

    virtual std::uint16_t block_size() const = 0;
};

} // namespace openads::drivers
