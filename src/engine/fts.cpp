#include "engine/fts.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
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

util::Result<Fts::Postings>
Fts::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return util::Error{5066, 0, "FTS file not found", path};
    }
    Postings out;
    std::string line;
    bool saw_header = false;
    while (std::getline(in, line)) {
        // Strip trailing CR if the file was written with CRLF line endings.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == '#') {
            saw_header = true;
            continue;
        }
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string token = line.substr(0, tab);
        std::string list  = line.substr(tab + 1);
        std::vector<std::uint32_t>& vec = out[token];
        std::size_t i = 0;
        while (i < list.size()) {
            std::size_t j = list.find(',', i);
            if (j == std::string::npos) j = list.size();
            if (j > i) {
                std::uint32_t r = static_cast<std::uint32_t>(
                    std::strtoul(list.c_str() + i, nullptr, 10));
                vec.push_back(r);
            }
            i = j + 1;
        }
    }
    if (!saw_header) {
        return util::Error{5103, 0, "FTS file missing header", path};
    }
    return out;
}

std::vector<std::uint32_t>
Fts::search(const Postings& postings, const std::string& query,
            const FtsOptions& opts) {
    auto tokens = tokenise(query, opts);
    if (tokens.empty()) return {};

    // Bootstrap the result with the first token's recno list so the
    // intersection loop can shrink it from there.
    std::vector<std::uint32_t> result;
    bool first = true;
    for (auto& tok : tokens) {
        auto it = postings.find(tok);
        if (it == postings.end()) return {};
        if (first) {
            result = it->second;
            first  = false;
        } else {
            std::vector<std::uint32_t> next;
            std::set_intersection(result.begin(), result.end(),
                                  it->second.begin(), it->second.end(),
                                  std::back_inserter(next));
            result = std::move(next);
            if (result.empty()) return result;
        }
    }
    return result;
}

} // namespace openads::engine
