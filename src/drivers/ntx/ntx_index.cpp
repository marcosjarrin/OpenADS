#include "drivers/ntx/ntx_index.h"

#include <algorithm>
#include <cstring>

namespace openads::drivers::ntx {

namespace {

std::uint16_t read_u16_le(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1] << 8);
}

void write_u16_le(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>( v       & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

std::uint32_t read_u32_le(const std::uint8_t* p) {
    return  static_cast<std::uint32_t>(p[0])        |
           (static_cast<std::uint32_t>(p[1]) <<  8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

void write_u32_le(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>( v        & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >>  8) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    p[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
}

std::string trim_nul(const std::uint8_t* p, std::size_t n) {
    std::size_t len = 0;
    while (len < n && p[len] != 0) ++len;
    return std::string(reinterpret_cast<const char*>(p), len);
}

platform::OpenMode map_mode(IndexOpenMode m) {
    switch (m) {
        case IndexOpenMode::ReadOnly:  return platform::OpenMode::ReadOnly;
        case IndexOpenMode::Shared:    return platform::OpenMode::OpenExisting;
        case IndexOpenMode::Exclusive: return platform::OpenMode::OpenExisting;
    }
    return platform::OpenMode::ReadOnly;
}

} // namespace

std::uint16_t NtxIndex::get_key_count(const Page& p) {
    return read_u16_le(p.data());
}

void NtxIndex::set_key_count(Page& p, std::uint16_t n) {
    write_u16_le(p.data(), n);
}

std::uint16_t NtxIndex::get_key_offset(const Page& p, std::int32_t i) {
    return read_u16_le(p.data() + 2 + i * 2);
}

void NtxIndex::set_key_offset(Page& p, std::int32_t i, std::uint16_t off) {
    write_u16_le(p.data() + 2 + i * 2, off);
}

std::uint32_t NtxIndex::get_left_child(const Page& p, std::int32_t i) {
    return read_u32_le(p.data() + get_key_offset(p, i));
}

std::uint32_t NtxIndex::get_recno(const Page& p, std::int32_t i) {
    return read_u32_le(p.data() + get_key_offset(p, i) + 4);
}

const std::uint8_t* NtxIndex::get_key_data(const Page& p, std::int32_t i) {
    return p.data() + get_key_offset(p, i) + 8;
}

void NtxIndex::put_left_child(Page& p, std::int32_t i, std::uint32_t v) {
    write_u32_le(p.data() + get_key_offset(p, i), v);
}

void NtxIndex::put_recno(Page& p, std::int32_t i, std::uint32_t v) {
    write_u32_le(p.data() + get_key_offset(p, i) + 4, v);
}

util::Result<NtxIndex::Page*> NtxIndex::get_page_(std::uint32_t offset) {
    auto it = page_cache_.find(offset);
    if (it != page_cache_.end()) return &it->second;
    Page p{};
    auto got = file_.read_at(offset, p.data(), p.size());
    if (!got) return got.error();
    if (got.value() < p.size()) {
        return util::Error{6106, 0, "short read on NTX page", ""};
    }
    auto [ins, _] = page_cache_.emplace(offset, p);
    return &ins->second;
}

util::Result<void> NtxIndex::flush_page_(std::uint32_t offset) {
    auto it = page_cache_.find(offset);
    if (it == page_cache_.end()) return {};
    auto wrote = file_.write_at(offset, it->second.data(), it->second.size());
    if (!wrote) return wrote.error();
    if (wrote.value() != it->second.size()) {
        return util::Error{5000, 0, "short write on NTX page", ""};
    }
    dirty_[offset] = false;
    return {};
}

util::Result<void> NtxIndex::open(const std::string& path, IndexOpenMode mode) {
    mode_ = mode;
    auto fres = platform::File::open(path, map_mode(mode));
    if (!fres) return fres.error();
    file_ = std::move(fres).value();

    Page hdr{};
    auto got = file_.read_at(0, hdr.data(), hdr.size());
    if (!got) return got.error();
    if (got.value() < NTX_PAGE_SIZE) {
        return util::Error{6106, 0, "NTX header truncated", path};
    }

    // Layout per Clipper / Harbour hbrddntx.h NTXHEADER.
    root_page_  = read_u32_le(hdr.data() + 4);
    next_avail_ = read_u32_le(hdr.data() + 8);
    item_size_  = read_u16_le(hdr.data() + 12);
    key_size_   = read_u16_le(hdr.data() + 14);
    /* key_dec  = read_u16_le(hdr.data() + 16); */
    max_keys_   = read_u16_le(hdr.data() + 18);
    half_page_  = read_u16_le(hdr.data() + 20);
    key_expr_   = trim_nul(hdr.data() + 22, 256);
    unique_     = hdr[278] != 0;
    descend_    = hdr[280] != 0;
    for_expr_   = trim_nul(hdr.data() + 282, 256);
    tag_name_   = trim_nul(hdr.data() + 538, 12);

    return {};
}

util::Result<void> NtxIndex::load_current_key_() {
    if (stack_.empty()) {
        current_key_.clear();
        current_recno_ = 0;
        return {};
    }
    const auto& frame = stack_.back();
    auto page = get_page_(frame.page);
    if (!page) return page.error();
    Page& p = *page.value();
    current_recno_ = get_recno(p, frame.key_index);
    current_key_.assign(reinterpret_cast<const char*>(get_key_data(p, frame.key_index)),
                        key_size_);
    return {};
}

util::Result<SeekOutcome>
NtxIndex::descend_leftmost_(std::uint32_t root) {
    stack_.clear();
    if (root == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
    std::uint32_t cur = root;
    while (cur != 0) {
        auto page = get_page_(cur);
        if (!page) return page.error();
        Page& p = *page.value();
        std::uint16_t kc = get_key_count(p);
        if (kc == 0) {
            return SeekOutcome{SeekHit::AfterEnd, 0, false};
        }
        // Descend through entry 0's left_child if non-zero (internal node).
        std::uint32_t child = get_left_child(p, 0);
        if (child == 0) {
            // Leaf: position on first key.
            stack_.push_back({cur, 0});
            if (auto r = load_current_key_(); !r) return r.error();
            return SeekOutcome{SeekHit::Exact, current_recno_, true};
        }
        stack_.push_back({cur, 0});
        cur = child;
    }
    return SeekOutcome{SeekHit::AfterEnd, 0, false};
}

util::Result<SeekOutcome>
NtxIndex::descend_rightmost_(std::uint32_t root) {
    stack_.clear();
    if (root == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
    std::uint32_t cur = root;
    while (cur != 0) {
        auto page = get_page_(cur);
        if (!page) return page.error();
        Page& p = *page.value();
        std::uint16_t kc = get_key_count(p);
        if (kc == 0) {
            return SeekOutcome{SeekHit::AfterEnd, 0, false};
        }
        // Right-most child is in slot kc (the trailing left_child).
        std::uint32_t child = get_left_child(p, kc);
        if (child == 0) {
            stack_.push_back({cur, kc - 1});
            if (auto r = load_current_key_(); !r) return r.error();
            return SeekOutcome{SeekHit::Exact, current_recno_, true};
        }
        stack_.push_back({cur, kc});
        cur = child;
    }
    return SeekOutcome{SeekHit::AfterEnd, 0, false};
}

util::Result<SeekOutcome> NtxIndex::seek_first() {
    return descend_leftmost_(root_page_);
}

util::Result<SeekOutcome> NtxIndex::seek_last() {
    return descend_rightmost_(root_page_);
}

util::Result<SeekOutcome>
NtxIndex::seek_key(const std::string& key, bool soft) {
    stack_.clear();
    if (root_page_ == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
    std::string padded = key;
    if (padded.size() < key_size_) padded.append(key_size_ - padded.size(), ' ');
    else if (padded.size() > key_size_) padded.resize(key_size_);

    std::uint32_t cur = root_page_;
    bool found_exact = false;
    while (cur != 0) {
        auto page = get_page_(cur);
        if (!page) return page.error();
        Page& p = *page.value();
        std::uint16_t kc = get_key_count(p);
        // Linear search for first key >= target. (Binary search optional.)
        std::int32_t i = 0;
        while (i < kc) {
            const std::uint8_t* kdata = get_key_data(p, i);
            int cmp = std::memcmp(padded.data(), kdata, key_size_);
            if (cmp == 0) { found_exact = true; break; }
            if (cmp < 0)  break;
            ++i;
        }
        std::uint32_t child = get_left_child(p, i);
        if (child == 0) {
            // Leaf
            if (i >= kc) {
                if (!soft) return SeekOutcome{SeekHit::AfterEnd, 0, false};
                if (kc == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
                stack_.push_back({cur, kc - 1});
                if (auto r = load_current_key_(); !r) return r.error();
                return SeekOutcome{SeekHit::AfterKey, current_recno_, true};
            }
            stack_.push_back({cur, i});
            if (auto r = load_current_key_(); !r) return r.error();
            return SeekOutcome{found_exact ? SeekHit::Exact : SeekHit::AfterKey,
                               current_recno_, true};
        }
        stack_.push_back({cur, i});
        if (found_exact) {
            if (auto r = load_current_key_(); !r) return r.error();
            return SeekOutcome{SeekHit::Exact, current_recno_, true};
        }
        cur = child;
    }
    return SeekOutcome{SeekHit::AfterEnd, 0, false};
}

util::Result<SeekOutcome> NtxIndex::next() {
    if (stack_.empty()) return SeekOutcome{SeekHit::AfterEnd, 0, false};

    // First try descending into the right subtree of current key.
    auto& top = stack_.back();
    auto page = get_page_(top.page);
    if (!page) return page.error();
    std::uint32_t right_child = get_left_child(*page.value(), top.key_index + 1);
    if (right_child != 0) {
        // Descend leftmost of right child.
        stack_.back().key_index += 1;  // Move to slot pointing at right child.
        std::uint32_t cur = right_child;
        while (cur != 0) {
            auto pg = get_page_(cur);
            if (!pg) return pg.error();
            Page& pp = *pg.value();
            std::uint16_t kc = get_key_count(pp);
            if (kc == 0) break;
            std::uint32_t child = get_left_child(pp, 0);
            if (child == 0) {
                stack_.push_back({cur, 0});
                if (auto r = load_current_key_(); !r) return r.error();
                return SeekOutcome{SeekHit::Exact, current_recno_, true};
            }
            stack_.push_back({cur, 0});
            cur = child;
        }
    }

    // Otherwise pop until we find a frame whose key_index < kc - 1.
    while (!stack_.empty()) {
        auto& f = stack_.back();
        auto pg = get_page_(f.page);
        if (!pg) return pg.error();
        Page& pp = *pg.value();
        std::uint16_t kc = get_key_count(pp);
        if (f.key_index + 1 < static_cast<std::int32_t>(kc)) {
            f.key_index += 1;
            if (auto r = load_current_key_(); !r) return r.error();
            return SeekOutcome{SeekHit::Exact, current_recno_, true};
        }
        stack_.pop_back();
    }
    return SeekOutcome{SeekHit::AfterEnd, 0, false};
}

util::Result<SeekOutcome> NtxIndex::prev() {
    if (stack_.empty()) return SeekOutcome{SeekHit::BeforeBegin, 0, false};

    // Try descending rightmost of left subtree of current key.
    auto& top = stack_.back();
    auto page = get_page_(top.page);
    if (!page) return page.error();
    std::uint32_t left_child = get_left_child(*page.value(), top.key_index);
    if (left_child != 0) {
        std::uint32_t cur = left_child;
        while (cur != 0) {
            auto pg = get_page_(cur);
            if (!pg) return pg.error();
            Page& pp = *pg.value();
            std::uint16_t kc = get_key_count(pp);
            if (kc == 0) break;
            std::uint32_t child = get_left_child(pp, kc);
            if (child == 0) {
                stack_.push_back({cur, kc - 1});
                if (auto r = load_current_key_(); !r) return r.error();
                return SeekOutcome{SeekHit::Exact, current_recno_, true};
            }
            stack_.push_back({cur, kc});
            cur = child;
        }
    }

    // Otherwise pop until we find a frame whose key_index > 0.
    while (!stack_.empty()) {
        auto& f = stack_.back();
        if (f.key_index > 0) {
            f.key_index -= 1;
            if (auto r = load_current_key_(); !r) return r.error();
            return SeekOutcome{SeekHit::Exact, current_recno_, true};
        }
        stack_.pop_back();
    }
    return SeekOutcome{SeekHit::BeforeBegin, 0, false};
}

util::Result<void> NtxIndex::insert(std::uint32_t /*recno*/,
                                    const std::string& /*key*/) {
    return util::Error{5004, 0, "NtxIndex::insert pending Task 3", ""};
}

util::Result<void> NtxIndex::erase(std::uint32_t /*recno*/,
                                   const std::string& /*key*/) {
    return util::Error{5004, 0, "NtxIndex::erase pending Task 3", ""};
}

util::Result<void> NtxIndex::flush() {
    for (auto& [off, _] : page_cache_) {
        auto r = flush_page_(off);
        if (!r) return r.error();
    }
    return file_.sync();
}

util::Result<NtxIndex>
NtxIndex::create(const std::string& /*path*/,
                 const std::string& /*tag_name*/,
                 const std::string& /*key_expr*/,
                 std::uint16_t      /*key_size*/,
                 bool               /*unique*/,
                 bool               /*descend*/) {
    return util::Error{5004, 0, "NtxIndex::create pending Task 3", ""};
}

} // namespace openads::drivers::ntx
