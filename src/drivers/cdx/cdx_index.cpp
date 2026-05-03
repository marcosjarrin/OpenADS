#include "drivers/cdx/cdx_index.h"

#include <algorithm>
#include <cstring>

namespace openads::drivers::cdx {

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

std::uint8_t bits_for(std::uint32_t n) {
    std::uint8_t b = 0;
    while (n) { ++b; n >>= 1; }
    return b == 0 ? 1 : b;
}

// FoxPro CDX leaf bit layout, mirrors Harbour hb_cdxPageLeafInitSpace
// (src/rdd/dbfcdx/dbfcdx1.c lines 1927-1941). bBits is derived from
// the key length; ReqByte / RNBits then fall out.
struct LeafLayout {
    std::uint8_t  req_byte;   // bytes per packed entry
    std::uint8_t  rn_bits;    // bits for record number
    std::uint8_t  dc_bits;    // bits for duplicate count (= bBits)
    std::uint8_t  tc_bits;    // bits for trailing count  (= bBits)
    std::uint32_t rn_mask;
    std::uint32_t dc_mask;
    std::uint32_t tc_mask;
};

LeafLayout compute_layout(std::uint16_t key_len) {
    LeafLayout out{};
    std::uint16_t v = key_len;
    std::uint8_t  b_bits = 0;
    while (v) { ++b_bits; v >>= 1; }
    out.dc_bits  = b_bits;
    out.tc_bits  = b_bits;
    out.req_byte = (b_bits > 12) ? 5 : (b_bits > 8 ? 4 : 3);
    out.rn_bits  = static_cast<std::uint8_t>(
        (out.req_byte << 3) - (b_bits << 1));
    out.dc_mask  = (b_bits >= 32) ? 0xFFFFFFFFu : ((1u << b_bits) - 1);
    out.tc_mask  = out.dc_mask;
    out.rn_mask  = (out.rn_bits >= 32) ? 0xFFFFFFFFu
                                       : ((1u << out.rn_bits) - 1);
    return out;
}

} // namespace

util::Result<CdxIndex::Page*> CdxIndex::get_page_(std::uint32_t offset) {
    auto it = page_cache_.find(offset);
    if (it != page_cache_.end()) return &it->second;
    Page p{};
    auto got = file_.read_at(offset, p.data(), p.size());
    if (!got) return got.error();
    if (got.value() < p.size()) {
        return util::Error{6106, 0, "short read on CDX page", ""};
    }
    auto [ins, _] = page_cache_.emplace(offset, p);
    return &ins->second;
}

util::Result<void> CdxIndex::flush_page_(std::uint32_t offset) {
    auto it = page_cache_.find(offset);
    if (it == page_cache_.end()) return {};
    auto wrote = file_.write_at(offset, it->second.data(), it->second.size());
    if (!wrote) return wrote.error();
    if (wrote.value() != it->second.size()) {
        return util::Error{5000, 0, "short write on CDX page", ""};
    }
    dirty_[offset] = false;
    return {};
}

util::Result<void>
CdxIndex::open(const std::string& path, IndexOpenMode mode) {
    mode_ = mode;
    auto fres = platform::File::open(path, map_mode(mode));
    if (!fres) return fres.error();
    file_ = std::move(fres).value();
    auto sz = file_.size();
    if (!sz) return sz.error();
    file_size_ = sz.value();

    // Read 1024-byte header (first two CDX pages).
    std::array<std::uint8_t, CDX_HEADER_LEN> hdr{};
    auto got = file_.read_at(0, hdr.data(), hdr.size());
    if (!got) return got.error();
    if (got.value() < CDX_HEADER_LEN) {
        return util::Error{6106, 0, "CDX header truncated", path};
    }

    root_page_ = read_u32_le(hdr.data() + 0);
    free_ptr_  = read_u32_le(hdr.data() + 4);
    counter_   = read_u32_le(hdr.data() + 8);
    key_size_  = read_u16_le(hdr.data() + 12);
    index_opt_ = hdr[14];
    index_sig_ = hdr[15];
    unique_    = (index_opt_ & 0x01) != 0;
    descend_   = read_u16_le(hdr.data() + 502) != 0;

    std::uint16_t kep_pos = read_u16_le(hdr.data() + 508);
    std::uint16_t kep_len = read_u16_le(hdr.data() + 510);
    if (kep_pos >= 512 && kep_pos + kep_len <= CDX_HEADER_LEN) {
        key_expr_.assign(reinterpret_cast<const char*>(hdr.data() + kep_pos),
                         kep_len);
    } else {
        key_expr_ = trim_nul(hdr.data() + 512, 256);
    }
    // Tag name lives nowhere in CDXTAGHEADER directly for sub-tags; we
    // stash it in reserved2 (bytes 24-91) by convention so round-trip
    // through create + open works.
    tag_name_ = trim_nul(hdr.data() + 24, 11);

    return {};
}

util::Result<std::vector<std::pair<std::string, std::uint32_t>>>
CdxIndex::decode_leaf_(std::uint32_t page_off) {
    auto page = get_page_(page_off);
    if (!page) return page.error();
    Page& p = *page.value();

    std::uint16_t attr = read_u16_le(p.data() + 0);
    if (!(attr & CDX_NODE_LEAF)) {
        return util::Error{6106, 0, "decode_leaf_ on non-leaf page", ""};
    }
    std::uint16_t nkeys     = read_u16_le(p.data() + 2);
    std::uint32_t rec_mask  = read_u32_le(p.data() + 14);
    std::uint8_t  dup_mask  = p[18];
    std::uint8_t  trl_mask  = p[19];
    std::uint8_t  dup_bits  = p[21];
    std::uint8_t  trl_bits  = p[22];
    std::uint8_t  key_bytes = p[23];
    (void)p[20]; // rec_bits — implied by key_bytes / dup_bits / trl_bits

    std::vector<std::pair<std::string, std::uint32_t>> out;
    out.reserve(nkeys);

    // bufKeyPos walks a position within the [CDX_EXT_HEADSIZE .. CDX_PAGE_LEN)
    // window backward from the page tail.
    std::uint32_t buf_pos = CDX_PAGE_LEN - CDX_EXT_HEADSIZE;
    std::string   prev(key_size_, ' ');

    for (std::uint16_t i = 0; i < nkeys; ++i) {
        std::uint8_t* entry = p.data() + CDX_EXT_HEADSIZE + i * key_bytes;
        if (entry + key_bytes > p.data() + CDX_PAGE_LEN) {
            return util::Error{6106, 0, "CDX leaf entry overruns page", ""};
        }

        // recno = LE32(entry) & rec_mask
        std::uint32_t recno = read_u32_le(entry) & rec_mask;
        // tmp = LE32(entry + key_bytes - 4) >> (32 - dup_bits - trl_bits)
        std::uint8_t  shift = static_cast<std::uint8_t>(
            32 - dup_bits - trl_bits);
        std::uint32_t tmp = read_u32_le(entry + key_bytes - 4) >> shift;
        std::uint32_t trl = tmp & trl_mask;
        std::uint32_t dup = (tmp >> trl_bits) & dup_mask;

        if (dup > key_size_ || trl > key_size_ || dup + trl > key_size_) {
            return util::Error{6106, 0, "CDX dup/trl out of range", ""};
        }
        std::uint32_t suffix_len = key_size_ - dup - trl;
        if (buf_pos < suffix_len) {
            return util::Error{6106, 0, "CDX suffix area underrun", ""};
        }
        buf_pos -= suffix_len;

        std::string key = prev;
        if (suffix_len > 0) {
            std::memcpy(key.data() + dup,
                        p.data() + CDX_EXT_HEADSIZE + buf_pos,
                        suffix_len);
        }
        // Pad trailing positions with space (bTrail).
        for (std::uint32_t t = key_size_ - trl; t < key_size_; ++t) {
            key[t] = ' ';
        }
        prev = key;
        out.emplace_back(key, recno);
    }
    return out;
}

util::Result<void>
CdxIndex::encode_leaf_(std::uint32_t page_off,
                       const std::vector<std::pair<std::string, std::uint32_t>>& keys,
                       std::uint32_t left_sib,
                       std::uint32_t right_sib) {
    auto page = get_page_(page_off);
    if (!page) return page.error();
    Page& p = *page.value();

    // FoxPro-derived layout (matches Harbour hb_cdxPageLeafInitSpace).
    LeafLayout L = compute_layout(key_size_);
    const std::uint32_t rec_mask = L.rn_mask;
    const std::uint8_t  rec_bits = L.rn_bits;
    const std::uint8_t  dup_bits = L.dc_bits;
    const std::uint8_t  trl_bits = L.tc_bits;
    const std::uint32_t dup_mask = L.dc_mask;
    const std::uint32_t trl_mask = L.tc_mask;
    const std::uint8_t  key_bytes = L.req_byte;

    std::fill(p.begin(), p.end(), std::uint8_t{0});
    write_u16_le(p.data() + 0, CDX_NODE_LEAF);
    write_u16_le(p.data() + 2, static_cast<std::uint16_t>(keys.size()));
    write_u32_le(p.data() + 4,  left_sib);
    write_u32_le(p.data() + 8,  right_sib);
    write_u32_le(p.data() + 14, rec_mask);
    p[18] = static_cast<std::uint8_t>(dup_mask & 0xFFu);
    p[19] = static_cast<std::uint8_t>(trl_mask & 0xFFu);
    p[20] = rec_bits;
    p[21] = dup_bits;
    p[22] = trl_bits;
    p[23] = key_bytes;

    // Build entries forward from offset 24, suffix data backward from page tail.
    std::uint32_t buf_pos = CDX_PAGE_LEN - CDX_EXT_HEADSIZE;
    std::string prev(key_size_, ' ');

    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto& [key, recno] = keys[i];
        std::string padded = key;
        if (padded.size() < key_size_) padded.append(key_size_ - padded.size(), ' ');
        if (padded.size() > key_size_) padded.resize(key_size_);

        std::uint32_t dup = 0;
        if (i > 0) {
            while (dup < key_size_ && padded[dup] == prev[dup]) ++dup;
        }
        std::uint32_t trl = 0;
        while (trl < key_size_ - dup && padded[key_size_ - 1 - trl] == ' ') ++trl;
        std::uint32_t suffix_len = key_size_ - dup - trl;

        // Write suffix bytes into the descending data area.
        if (buf_pos < suffix_len) {
            return util::Error{5000, 0, "CDX leaf encode: page full", ""};
        }
        buf_pos -= suffix_len;
        if (suffix_len > 0) {
            std::memcpy(p.data() + CDX_EXT_HEADSIZE + buf_pos,
                        padded.data() + dup, suffix_len);
        }

        // Pack entry — mirrors Harbour hb_cdxSetLeafRecord (dbfcdx1.c
        // lines 1743-1761). Lay recno bytes LE across `key_bytes`,
        // then OR dup+trl bits into the upper bytes such that decoded
        // tmp = LE32(entry + key_bytes - 4) >> (32 - tc_bits - dc_bits)
        // yields trl in the high portion and dup in the low portion.
        std::uint8_t* entry = p.data() + CDX_EXT_HEADSIZE + i * key_bytes;
        if (entry + key_bytes > p.data() + CDX_PAGE_LEN) {
            return util::Error{5000, 0, "CDX leaf entry overflow", ""};
        }

        std::uint32_t bits = ((trl & trl_mask) << dup_bits) | (dup & dup_mask);
        const int top_bytes = (trl_bits + dup_bits + 7) >> 3;
        const int from_byte = key_bytes - top_bytes;
        bits <<= ((top_bytes << 3) - trl_bits - dup_bits);

        std::uint32_t rec = recno & rec_mask;
        for (int byte_i = 0; byte_i < key_bytes; ++byte_i) {
            std::uint8_t b = static_cast<std::uint8_t>(rec & 0xFFu);
            rec >>= 8;
            if (byte_i >= from_byte) {
                b = static_cast<std::uint8_t>(b | (bits & 0xFFu));
                bits >>= 8;
            }
            entry[byte_i] = b;
        }

        prev = padded;
    }

    // Free space tracker: between end of entries and start of suffix data.
    std::uint32_t entries_end = CDX_EXT_HEADSIZE +
        static_cast<std::uint32_t>(keys.size()) * key_bytes;
    std::uint32_t suffix_start = CDX_EXT_HEADSIZE + buf_pos;
    std::uint32_t free_spc = (suffix_start > entries_end)
        ? (suffix_start - entries_end) : 0;
    write_u16_le(p.data() + 12, static_cast<std::uint16_t>(free_spc));
    dirty_[page_off] = true;
    return {};
}

util::Result<SeekOutcome> CdxIndex::seek_first() {
    cur_leaf_   = 0;
    cur_index_  = -1;
    cur_decoded_.clear();
    if (root_page_ == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};

    // Descend leftmost leaf: when root is a leaf, use it directly.
    auto page = get_page_(root_page_);
    if (!page) return page.error();
    std::uint16_t attr = read_u16_le(page.value()->data());
    if (attr & CDX_NODE_LEAF) {
        cur_leaf_ = root_page_;
    } else {
        // Branch descent (left edge).
        std::uint32_t cur = root_page_;
        while (true) {
            auto pg = get_page_(cur);
            if (!pg) return pg.error();
            std::uint16_t at = read_u16_le(pg.value()->data());
            if (at & CDX_NODE_LEAF) { cur_leaf_ = cur; break; }
            // Branch entries: each is keySize+8 bytes; first child at
            // offset 12 + keySize + 4.
            std::uint8_t* base = pg.value()->data();
            std::uint16_t nkeys = read_u16_le(base + 2);
            if (nkeys == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
            cur = read_u32_le(base + CDX_INT_HEADSIZE + key_size_ + 4);
        }
    }

    auto dec = decode_leaf_(cur_leaf_);
    if (!dec) return dec.error();
    cur_decoded_ = std::move(dec).value();
    if (cur_decoded_.empty()) {
        return SeekOutcome{SeekHit::AfterEnd, 0, false};
    }
    cur_index_ = 0;
    current_key_ = cur_decoded_[0].first;
    return SeekOutcome{SeekHit::Exact, cur_decoded_[0].second, true};
}

util::Result<SeekOutcome> CdxIndex::seek_last() {
    if (root_page_ == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
    cur_leaf_ = 0; cur_index_ = -1; cur_decoded_.clear();

    // Walk leaf siblings until rightPtr == -1.
    auto first = seek_first();
    if (!first) return first.error();
    if (!first.value().positioned) return first;

    while (true) {
        auto pg = get_page_(cur_leaf_);
        if (!pg) return pg.error();
        std::uint32_t right = read_u32_le(pg.value()->data() + 8);
        if (right == 0xFFFFFFFFu || right == 0) break;
        cur_leaf_ = right;
        auto dec = decode_leaf_(cur_leaf_);
        if (!dec) return dec.error();
        cur_decoded_ = std::move(dec).value();
        if (cur_decoded_.empty()) break;
    }
    if (cur_decoded_.empty()) {
        return SeekOutcome{SeekHit::AfterEnd, 0, false};
    }
    cur_index_ = static_cast<std::int32_t>(cur_decoded_.size() - 1);
    current_key_ = cur_decoded_[cur_index_].first;
    return SeekOutcome{SeekHit::Exact, cur_decoded_[cur_index_].second, true};
}

util::Result<SeekOutcome>
CdxIndex::seek_key(const std::string& key, bool soft) {
    if (root_page_ == 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
    std::string padded = key;
    if (padded.size() < key_size_) padded.append(key_size_ - padded.size(), ' ');
    if (padded.size() > key_size_) padded.resize(key_size_);

    auto first = seek_first();
    if (!first) return first.error();
    if (!first.value().positioned) {
        return SeekOutcome{SeekHit::AfterEnd, 0, false};
    }

    while (true) {
        for (std::size_t i = 0; i < cur_decoded_.size(); ++i) {
            int cmp = std::memcmp(padded.data(), cur_decoded_[i].first.data(),
                                  key_size_);
            if (cmp == 0) {
                cur_index_ = static_cast<std::int32_t>(i);
                current_key_ = cur_decoded_[i].first;
                return SeekOutcome{SeekHit::Exact, cur_decoded_[i].second, true};
            }
            if (cmp < 0) {
                if (!soft) return SeekOutcome{SeekHit::AfterEnd, 0, false};
                cur_index_ = static_cast<std::int32_t>(i);
                current_key_ = cur_decoded_[i].first;
                return SeekOutcome{SeekHit::AfterKey, cur_decoded_[i].second, true};
            }
        }
        // Move to next sibling.
        auto pg = get_page_(cur_leaf_);
        if (!pg) return pg.error();
        std::uint32_t right = read_u32_le(pg.value()->data() + 8);
        if (right == 0xFFFFFFFFu || right == 0) break;
        cur_leaf_ = right;
        auto dec = decode_leaf_(cur_leaf_);
        if (!dec) return dec.error();
        cur_decoded_ = std::move(dec).value();
        if (cur_decoded_.empty()) break;
    }
    return SeekOutcome{SeekHit::AfterEnd, 0, false};
}

util::Result<SeekOutcome> CdxIndex::next() {
    if (cur_index_ < 0) return SeekOutcome{SeekHit::AfterEnd, 0, false};
    if (static_cast<std::size_t>(cur_index_ + 1) < cur_decoded_.size()) {
        cur_index_ += 1;
        current_key_ = cur_decoded_[cur_index_].first;
        return SeekOutcome{SeekHit::Exact, cur_decoded_[cur_index_].second, true};
    }
    auto pg = get_page_(cur_leaf_);
    if (!pg) return pg.error();
    std::uint32_t right = read_u32_le(pg.value()->data() + 8);
    if (right == 0xFFFFFFFFu || right == 0) {
        cur_index_ = -1;
        return SeekOutcome{SeekHit::AfterEnd, 0, false};
    }
    cur_leaf_ = right;
    auto dec = decode_leaf_(cur_leaf_);
    if (!dec) return dec.error();
    cur_decoded_ = std::move(dec).value();
    if (cur_decoded_.empty()) {
        cur_index_ = -1;
        return SeekOutcome{SeekHit::AfterEnd, 0, false};
    }
    cur_index_ = 0;
    current_key_ = cur_decoded_[0].first;
    return SeekOutcome{SeekHit::Exact, cur_decoded_[0].second, true};
}

util::Result<SeekOutcome> CdxIndex::prev() {
    if (cur_index_ < 0) return SeekOutcome{SeekHit::BeforeBegin, 0, false};
    if (cur_index_ > 0) {
        cur_index_ -= 1;
        current_key_ = cur_decoded_[cur_index_].first;
        return SeekOutcome{SeekHit::Exact, cur_decoded_[cur_index_].second, true};
    }
    auto pg = get_page_(cur_leaf_);
    if (!pg) return pg.error();
    std::uint32_t left = read_u32_le(pg.value()->data() + 4);
    if (left == 0xFFFFFFFFu || left == 0) {
        cur_index_ = -1;
        return SeekOutcome{SeekHit::BeforeBegin, 0, false};
    }
    cur_leaf_ = left;
    auto dec = decode_leaf_(cur_leaf_);
    if (!dec) return dec.error();
    cur_decoded_ = std::move(dec).value();
    if (cur_decoded_.empty()) {
        cur_index_ = -1;
        return SeekOutcome{SeekHit::BeforeBegin, 0, false};
    }
    cur_index_ = static_cast<std::int32_t>(cur_decoded_.size() - 1);
    current_key_ = cur_decoded_[cur_index_].first;
    return SeekOutcome{SeekHit::Exact, cur_decoded_[cur_index_].second, true};
}

util::Result<void>
CdxIndex::insert(std::uint32_t recno, const std::string& key) {
    if (mode_ == IndexOpenMode::ReadOnly) {
        return util::Error{5000, 0, "CDX opened read-only", ""};
    }
    std::string padded = key;
    if (padded.size() < key_size_) padded.append(key_size_ - padded.size(), ' ');
    if (padded.size() > key_size_) padded.resize(key_size_);

    if (root_page_ == 0) {
        // Allocate first leaf at next available offset.
        std::uint32_t off = static_cast<std::uint32_t>(
            file_size_ < CDX_HEADER_LEN ? CDX_HEADER_LEN : file_size_);
        page_cache_.emplace(off, Page{});
        std::vector<std::pair<std::string, std::uint32_t>> keys{{padded, recno}};
        if (auto e = encode_leaf_(off, keys, 0xFFFFFFFFu, 0xFFFFFFFFu); !e) {
            return e.error();
        }
        root_page_ = off;
        file_size_ = off + CDX_PAGE_LEN;
        return rewrite_header_();
    }

    auto dec = decode_leaf_(root_page_);
    if (!dec) return dec.error();
    auto keys = std::move(dec).value();

    // Insert sorted; reject duplicates if unique.
    auto pos = std::lower_bound(keys.begin(), keys.end(), padded,
        [](const auto& a, const std::string& b) {
            return std::memcmp(a.first.data(), b.data(),
                               std::min(a.first.size(), b.size())) < 0;
        });
    if (unique_ && pos != keys.end() && pos->first == padded) {
        return util::Error{5044, 0, "CDX duplicate key", ""};
    }
    keys.insert(pos, {padded, recno});

    auto enc = encode_leaf_(root_page_, keys, 0xFFFFFFFFu, 0xFFFFFFFFu);
    if (!enc) return enc.error();
    return {};
}

util::Result<void>
CdxIndex::erase(std::uint32_t recno, const std::string& key) {
    if (mode_ == IndexOpenMode::ReadOnly) {
        return util::Error{5000, 0, "CDX opened read-only", ""};
    }
    if (root_page_ == 0) return util::Error{5044, 0, "CDX empty", ""};

    auto dec = decode_leaf_(root_page_);
    if (!dec) return dec.error();
    auto keys = std::move(dec).value();

    std::string padded = key;
    if (padded.size() < key_size_) padded.append(key_size_ - padded.size(), ' ');
    if (padded.size() > key_size_) padded.resize(key_size_);

    auto it = std::find_if(keys.begin(), keys.end(),
        [&](const auto& kv) {
            return kv.first == padded && (recno == 0 || kv.second == recno);
        });
    if (it == keys.end()) {
        return util::Error{5044, 0, "CDX key not found", ""};
    }
    keys.erase(it);
    return encode_leaf_(root_page_, keys, 0xFFFFFFFFu, 0xFFFFFFFFu);
}

util::Result<void> CdxIndex::flush() {
    for (auto& [off, _] : page_cache_) {
        auto r = flush_page_(off);
        if (!r) return r.error();
    }
    return file_.sync();
}

util::Result<void> CdxIndex::rewrite_header_() {
    std::array<std::uint8_t, CDX_HEADER_LEN> hdr{};
    auto got = file_.read_at(0, hdr.data(), hdr.size());
    if (!got) return got.error();
    write_u32_le(hdr.data() + 0, root_page_);
    write_u32_le(hdr.data() + 4, free_ptr_);
    write_u32_le(hdr.data() + 8, ++counter_);
    auto wrote = file_.write_at(0, hdr.data(), hdr.size());
    if (!wrote) return wrote.error();
    return {};
}

util::Result<CdxIndex>
CdxIndex::create(const std::string& path,
                 const std::string& tag_name,
                 const std::string& key_expr,
                 std::uint16_t      key_size,
                 bool               unique,
                 bool               descend) {
    auto fres = platform::File::open(path, platform::OpenMode::CreateRW);
    if (!fres) return fres.error();
    platform::File file = std::move(fres).value();

    std::array<std::uint8_t, CDX_HEADER_LEN> hdr{};
    write_u32_le(hdr.data() + 0, 0);                 // root (none yet)
    write_u32_le(hdr.data() + 4, 0xFFFFFFFFu);       // freePtr
    write_u32_le(hdr.data() + 8, 1);                 // counter
    write_u16_le(hdr.data() + 12, key_size);         // keySize
    hdr[14] = static_cast<std::uint8_t>(             // indexOpt
        (unique ? 0x01 : 0x00) | 0x20);              // CDX_TYPE_COMPACT
    hdr[15] = 0x01;                                  // indexSig
    write_u16_le(hdr.data() + 16, CDX_HEADER_LEN);   // headerLen
    write_u16_le(hdr.data() + 18, CDX_PAGE_LEN);     // pageLen
    write_u16_le(hdr.data() + 502, descend ? 1 : 0); // ascendFlg
    write_u16_le(hdr.data() + 508, 512);             // keyExpPos
    write_u16_le(hdr.data() + 510,
                 static_cast<std::uint16_t>(key_expr.size()));
    std::memcpy(hdr.data() + 512, key_expr.data(),
                std::min<std::size_t>(key_expr.size(), 511));
    // Stash tag name in reserved2 area for round-tripping.
    std::memcpy(hdr.data() + 24, tag_name.data(),
                std::min<std::size_t>(tag_name.size(), 11));

    auto wrote = file.write_at(0, hdr.data(), hdr.size());
    if (!wrote) return wrote.error();
    if (auto s = file.sync(); !s) return s.error();

    CdxIndex ix;
    ix.file_       = std::move(file);
    ix.mode_       = IndexOpenMode::Shared;
    ix.root_page_  = 0;
    ix.free_ptr_   = 0xFFFFFFFFu;
    ix.counter_    = 1;
    ix.key_size_   = key_size;
    ix.index_opt_  = static_cast<std::uint8_t>(
        (unique ? 0x01 : 0x00) | 0x20);
    ix.unique_     = unique;
    ix.descend_    = descend;
    ix.key_expr_   = key_expr;
    ix.tag_name_   = tag_name;
    ix.file_size_  = CDX_HEADER_LEN;
    return ix;
}

std::uint16_t CdxIndex::pick_rec_bits_(std::uint32_t max_rec) {
    return bits_for(max_rec);
}

} // namespace openads::drivers::cdx
