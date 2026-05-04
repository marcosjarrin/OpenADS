#include "engine/fts.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>

namespace openads::engine {

namespace {

bool is_default_delim(char c) {
    if (c <= ' ') return true;
    switch (c) {
        case '.': case ',': case ';': case ':': case '?': case '!':
        case '"': case '\'': case '(': case ')': case '[': case ']':
        case '{': case '}': case '<': case '>': case '/': case '\\':
        case '|': case '-': case '_': case '+': case '*': case '=':
        case '@': case '#': case '$': case '%': case '^': case '&':
        case '`': case '~':
            return true;
        default:
            return false;
    }
}

bool is_extra_delim(char c, const std::string& extra) {
    return extra.find(c) != std::string::npos;
}

}  // namespace

std::vector<std::string>
Fts::tokenise(const std::string& s, const FtsOptions& opts) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&] {
        if (cur.empty()) return;
        if (cur.size() >= opts.min_word_len &&
            cur.size() <= opts.max_word_len &&
            opts.noise_words.find(cur) == opts.noise_words.end()) {
            out.push_back(cur);
        }
        cur.clear();
    };
    for (char c : s) {
        if (is_default_delim(c) || is_extra_delim(c, opts.extra_delims)) {
            flush();
        } else {
            cur.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(c))));
        }
    }
    flush();
    return out;
}

util::Result<void>
Fts::create(Table& table, const std::string& path,
            const std::string& tag, const std::string& field_name,
            const FtsOptions& opts) {
    std::int32_t fidx = table.field_index(field_name);
    if (fidx < 0) {
        return util::Error{5063, 0, "FTS field not found", field_name};
    }

    std::map<std::string, std::set<std::uint32_t>> postings;

    std::uint32_t rc = table.record_count();
    for (std::uint32_t r = 1; r <= rc; ++r) {
        if (auto g = table.goto_record(r); !g) return g.error();
        if (table.is_deleted()) continue;
        auto v = table.read_field(static_cast<std::uint16_t>(fidx));
        if (!v) return v.error();
        for (auto& tok : tokenise(v.value().as_string, opts)) {
            postings[tok].insert(r);
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return util::Error{5000, 0, "cannot open FTS file for write", path};
    }
    out << "# OpenADS FTS v0 tag=" << tag
        << " field=" << field_name
        << " min="   << opts.min_word_len
        << " max="   << opts.max_word_len
        << "\n";
    for (auto& [tok, recs] : postings) {
        out << tok << '\t';
        bool first = true;
        for (auto r : recs) {
            if (!first) out << ',';
            out << r;
            first = false;
        }
        out << '\n';
    }
    out.flush();
    if (!out) {
        return util::Error{5000, 0, "FTS file write failed", path};
    }
    return {};
}

} // namespace openads::engine
