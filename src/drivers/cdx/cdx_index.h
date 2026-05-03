#pragma once

#include "drivers/index_trait.h"
#include "platform/file.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openads::drivers::cdx {

constexpr std::uint16_t CDX_PAGE_LEN     = 512;
constexpr std::uint16_t CDX_HEADER_LEN   = 1024;
constexpr std::uint16_t CDX_EXT_HEADSIZE = 24;
constexpr std::uint16_t CDX_INT_HEADSIZE = 12;

constexpr std::uint16_t CDX_NODE_BRANCH = 0;
constexpr std::uint16_t CDX_NODE_ROOT   = 1;
constexpr std::uint16_t CDX_NODE_LEAF   = 2;

class CdxIndex final : public IIndex {
public:
    util::Result<void> open(const std::string& path, IndexOpenMode mode) override;

    std::string name()       const override { return tag_name_; }
    std::string expression() const override { return key_expr_; }
    bool        descending() const override { return descend_; }
    bool        unique()     const override { return unique_; }
    std::uint16_t key_length() const override { return key_size_; }

    util::Result<SeekOutcome> seek_first() override;
    util::Result<SeekOutcome> seek_last()  override;
    util::Result<SeekOutcome>
        seek_key(const std::string& key, bool soft) override;
    util::Result<SeekOutcome> next()       override;
    util::Result<SeekOutcome> prev()       override;
    std::string current_key() const override { return current_key_; }

    util::Result<void> insert(std::uint32_t recno,
                              const std::string& key) override;
    util::Result<void> erase (std::uint32_t recno,
                              const std::string& key) override;
    util::Result<void> flush() override;

    // Build a fresh single-tag CDX on disk. Layout: page 0 is the
    // (only) tag header; the first leaf is allocated lazily on first
    // insert. Real FoxPro compound CDX (with a structure tag) is a
    // future extension.
    static util::Result<CdxIndex>
        create(const std::string& path,
               const std::string& tag_name,
               const std::string& key_expr,
               std::uint16_t      key_size,
               bool               unique,
               bool               descend);

    using Page = std::array<std::uint8_t, CDX_PAGE_LEN>;

private:
    util::Result<Page*> get_page_(std::uint32_t offset);
    util::Result<void>  flush_page_(std::uint32_t offset);

    util::Result<std::vector<std::pair<std::string, std::uint32_t>>>
        decode_leaf_(std::uint32_t page_off);

    util::Result<void>
        encode_leaf_(std::uint32_t page_off,
                     const std::vector<std::pair<std::string, std::uint32_t>>& keys,
                     std::uint32_t left_sib,
                     std::uint32_t right_sib);

    static std::uint16_t pick_rec_bits_(std::uint32_t max_rec);

    util::Result<void> rewrite_header_();

    platform::File                          file_;
    IndexOpenMode                           mode_      = IndexOpenMode::ReadOnly;
    std::uint32_t                           root_page_ = 0;
    std::uint32_t                           free_ptr_  = 0xFFFFFFFFu;
    std::uint32_t                           counter_   = 0;
    std::uint16_t                           key_size_  = 0;
    std::uint8_t                            index_opt_ = 0;
    std::uint8_t                            index_sig_ = 0x01;
    bool                                    unique_    = false;
    bool                                    descend_   = false;
    std::string                             key_expr_;
    std::string                             tag_name_;
    std::uint64_t                           file_size_ = 0;

    std::unordered_map<std::uint32_t, Page> page_cache_;
    std::unordered_map<std::uint32_t, bool> dirty_;

    // Cursor: a single (leaf_page, key_index_in_leaf) plus the cached
    // decoded keys for that leaf.
    std::uint32_t                                                 cur_leaf_      = 0;
    std::int32_t                                                  cur_index_     = -1;
    std::vector<std::pair<std::string, std::uint32_t>>            cur_decoded_;
    std::string                                                   current_key_;
};

} // namespace openads::drivers::cdx
