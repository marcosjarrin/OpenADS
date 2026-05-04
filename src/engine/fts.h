#pragma once

#include "engine/table.h"
#include "util/result.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openads::engine {

// OpenADS-native full-text search index (M9.19).
//
// File format is plain UTF-8 text — clean-room, not derived from any
// proprietary ADS FTS layout. A reader can rebuild the inverted map
// from the file in one pass without an SDK or RE work:
//
//   # OpenADS FTS v0 tag=<tag> field=<col> min=<n> max=<n>
//   <token>\t<recno1>,<recno2>,...
//   <token>\t<recno1>,...
//   ...
//
// Tokens are emitted in sorted order; recnos are de-duplicated and
// emitted in ascending order so a `git diff` over two builds is
// stable.

struct FtsOptions {
    std::uint32_t                min_word_len   = 3;
    std::uint32_t                max_word_len   = 30;
    std::string                  extra_delims;     // ASCII characters treated as separators
    std::unordered_set<std::string> noise_words;   // skipped tokens (already lowercased)
};

class Fts {
public:
    // Build an inverted-index file at `path` by walking every live
    // record of `table`, reading `field_name`, tokenising the value,
    // and emitting `(token, recnos[])` rows.
    static util::Result<void>
        create(Table&             table,
               const std::string& path,
               const std::string& tag,
               const std::string& field_name,
               const FtsOptions&  opts);

    // Tokenise a single string with the same rules `create` uses.
    // Exposed for tests so the splitter can be exercised independently.
    static std::vector<std::string>
        tokenise(const std::string& s, const FtsOptions& opts);

    // Load an OpenADS-native .fts file produced by `create` into an
    // in-memory inverted map (M9.21). Returns the map keyed by token,
    // mapping to the sorted recno list. Format errors and missing
    // files surface as Errors; a missing file is the common case for
    // first-call-after-AdsCreateFTSIndex apps and bubbles up so the
    // ABI can map it to AE_TABLE_NOT_FOUND.
    using Postings = std::unordered_map<std::string, std::vector<std::uint32_t>>;
    static util::Result<Postings>
        load(const std::string& path);

    // Tokenise `query`, look each token up in `postings`, and return
    // the intersection of their recno lists (AND semantics — every
    // token must appear in the matching record). An empty query, or
    // a query whose every token misses, returns an empty result.
    // Tokens shorter than 1 char are skipped before lookup; the
    // caller's `opts` (delim/min/max/noise) shape the same tokeniser
    // used at build time so a `create` + `search` pair stays
    // symmetric.
    static std::vector<std::uint32_t>
        search(const Postings&    postings,
               const std::string& query,
               const FtsOptions&  opts);
};

} // namespace openads::engine
