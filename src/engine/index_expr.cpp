#include "engine/index_expr.h"

#include "engine/table.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <variant>

namespace openads::engine {

namespace {

// ---- Tokenizer ------------------------------------------------------

enum class TokKind {
    Ident, Number, String, LParen, RParen, Comma, Plus, End
};

struct Token {
    TokKind     kind;
    std::string text;
};

std::vector<Token> tokenize(const std::string& s) {
    std::vector<Token> out;
    std::size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '(') { out.push_back({TokKind::LParen, "("}); ++i; continue; }
        if (c == ')') { out.push_back({TokKind::RParen, ")"}); ++i; continue; }
        if (c == ',') { out.push_back({TokKind::Comma,  ","}); ++i; continue; }
        if (c == '+') { out.push_back({TokKind::Plus,   "+"}); ++i; continue; }
        if (c == '"' || c == '\'') {
            char q = c; ++i;
            std::string lit;
            while (i < s.size() && s[i] != q) { lit.push_back(s[i++]); }
            if (i < s.size()) ++i;
            out.push_back({TokKind::String, std::move(lit)});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '-' && i + 1 < s.size() &&
             std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
            std::string num;
            if (c == '-') { num.push_back(c); ++i; }
            while (i < s.size() &&
                   (std::isdigit(static_cast<unsigned char>(s[i])) ||
                    s[i] == '.')) {
                num.push_back(s[i++]);
            }
            out.push_back({TokKind::Number, std::move(num)});
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string id;
            while (i < s.size() &&
                   (std::isalnum(static_cast<unsigned char>(s[i])) ||
                    s[i] == '_')) {
                id.push_back(s[i++]);
            }
            out.push_back({TokKind::Ident, std::move(id)});
            continue;
        }
        ++i;   // skip the unknown char rather than die — best-effort.
    }
    out.push_back({TokKind::End, ""});
    return out;
}

// ---- AST + evaluator ------------------------------------------------

struct Value {
    bool        is_number = false;
    std::string s;
    double      n = 0.0;
};

// ---- UTF-8 codepoint walker (M9.22) --------------------------------------
//
// UPPER / LOWER / SUBSTR walk codepoints instead of bytes so an index
// expression like UPPER(name) over a UTF-8 column normalises non-ASCII
// characters (ñ → Ñ, ç → Ç, é → É, …) instead of leaving the multi-byte
// sequence unchanged. The case-mapping table covers ASCII + Latin-1
// supplement + the U+00FF / U+0178 pair; codepoints outside that range
// pass through untouched (full Unicode case folding is ICU territory
// and out of scope until the engine carries an ICU dependency).

std::uint32_t utf8_decode_at(const std::string& s, std::size_t& i) {
    if (i >= s.size()) return 0xFFFD;
    unsigned char c = static_cast<unsigned char>(s[i]);
    std::uint32_t cp = 0xFFFD;
    std::size_t adv = 1;
    auto byte = [&](std::size_t k) {
        return static_cast<unsigned char>(s[k]);
    };
    if (c < 0x80) {
        cp = c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size() &&
               (byte(i + 1) & 0xC0) == 0x80) {
        cp = (static_cast<std::uint32_t>(c & 0x1F) << 6) |
              static_cast<std::uint32_t>(byte(i + 1) & 0x3F);
        adv = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size() &&
               (byte(i + 1) & 0xC0) == 0x80 &&
               (byte(i + 2) & 0xC0) == 0x80) {
        cp = (static_cast<std::uint32_t>(c & 0x0F) << 12) |
             (static_cast<std::uint32_t>(byte(i + 1) & 0x3F) << 6) |
              static_cast<std::uint32_t>(byte(i + 2) & 0x3F);
        adv = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size() &&
               (byte(i + 1) & 0xC0) == 0x80 &&
               (byte(i + 2) & 0xC0) == 0x80 &&
               (byte(i + 3) & 0xC0) == 0x80) {
        cp = (static_cast<std::uint32_t>(c & 0x07) << 18) |
             (static_cast<std::uint32_t>(byte(i + 1) & 0x3F) << 12) |
             (static_cast<std::uint32_t>(byte(i + 2) & 0x3F) << 6) |
              static_cast<std::uint32_t>(byte(i + 3) & 0x3F);
        adv = 4;
    }
    i += adv;
    return cp;
}

void utf8_encode(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

std::uint32_t to_upper_cp(std::uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 0x20;
    // Latin-1 supplement: à..þ → À..Þ, skipping ÷ at 0xF7.
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 0x20;
    if (cp == 0xFF) return 0x178;   // ÿ → Ÿ
    return cp;
}

std::uint32_t to_lower_cp(std::uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 0x20;
    if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) return cp + 0x20;
    if (cp == 0x178) return 0xFF;
    return cp;
}

std::string upper_utf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint32_t cp = utf8_decode_at(s, i);
        utf8_encode(out, to_upper_cp(cp));
    }
    return out;
}

std::string lower_utf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint32_t cp = utf8_decode_at(s, i);
        utf8_encode(out, to_lower_cp(cp));
    }
    return out;
}

std::string substr_utf8(const std::string& s, int start_1based, int len) {
    if (start_1based < 1) start_1based = 1;
    if (len < 0) len = 0;
    std::string out;
    std::size_t i = 0;
    int char_index = 0;       // 0-based codepoint counter
    int copied = 0;
    while (i < s.size() && copied < len) {
        std::size_t before = i;
        (void)utf8_decode_at(s, i);   // advance one codepoint
        if (char_index + 1 >= start_1based) {
            out.append(s, before, i - before);
            ++copied;
        }
        ++char_index;
        if (i == before) break;   // safety on malformed input
    }
    return out;
}

std::string ltrim_ascii(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
std::string rtrim_ascii(const std::string& s) {
    std::size_t e = s.size();
    while (e > 0 && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(0, e);
}

class Parser {
public:
    Parser(const std::vector<Token>& toks, Table& t)
        : toks_(toks), t_(t) {}

    Value parse_expr() {
        Value lhs = parse_term();
        while (peek().kind == TokKind::Plus) {
            ++pos_;
            Value rhs = parse_term();
            // String concatenation; numeric values are stringified
            // first (Clipper semantics for + on mixed operands are
            // unsupported in CDX expressions).
            std::string a = lhs.is_number ? format_number(lhs.n, 0, 0) : lhs.s;
            std::string b = rhs.is_number ? format_number(rhs.n, 0, 0) : rhs.s;
            lhs.is_number = false;
            lhs.s = a + b;
        }
        return lhs;
    }

private:
    const Token& peek() const { return toks_[pos_]; }

    Value parse_term() {
        const Token& tk = peek();
        if (tk.kind == TokKind::String) {
            ++pos_;
            Value v;
            v.is_number = false;
            v.s = tk.text;
            return v;
        }
        if (tk.kind == TokKind::Number) {
            ++pos_;
            Value v;
            v.is_number = true;
            v.n = std::strtod(tk.text.c_str(), nullptr);
            return v;
        }
        if (tk.kind == TokKind::LParen) {
            ++pos_;
            Value v = parse_expr();
            if (peek().kind == TokKind::RParen) ++pos_;
            return v;
        }
        if (tk.kind == TokKind::Ident) {
            std::string name = tk.text;
            ++pos_;
            if (peek().kind == TokKind::LParen) {
                ++pos_;
                std::vector<Value> args;
                if (peek().kind != TokKind::RParen) {
                    args.push_back(parse_expr());
                    while (peek().kind == TokKind::Comma) {
                        ++pos_;
                        args.push_back(parse_expr());
                    }
                }
                if (peek().kind == TokKind::RParen) ++pos_;
                return call(upper_utf8(name), args);
            }
            return field_or_empty(name);
        }
        Value v;
        v.is_number = false;
        return v;
    }

    Value field_or_empty(const std::string& name) {
        Value v;
        v.is_number = false;
        std::int32_t fidx = t_.field_index(name);
        if (fidx < 0) return v;
        auto r = t_.read_field(static_cast<std::uint16_t>(fidx));
        if (!r) return v;
        const auto& f = t_.field_descriptor(static_cast<std::uint16_t>(fidx));
        if (f.type == drivers::DbfFieldType::Numeric ||
            f.type == drivers::DbfFieldType::Float ||
            f.type == drivers::DbfFieldType::Integer ||
            f.type == drivers::DbfFieldType::Double ||
            f.type == drivers::DbfFieldType::Currency) {
            v.is_number = true;
            v.n = r.value().as_double;
            // For arithmetic-style indexes the Clipper convention is
            // STR(numeric, len). The bare-field path returns the
            // *string* representation matching the on-disk bytes.
            v.s = r.value().as_string;
        } else {
            v.is_number = false;
            v.s = r.value().as_string;
        }
        return v;
    }

    static std::string format_number(double v, int width, int dec) {
        char buf[64];
        if (width <= 0) {
            std::snprintf(buf, sizeof(buf), "%.*f", dec > 0 ? dec : 0, v);
        } else {
            std::snprintf(buf, sizeof(buf), "%*.*f", width,
                          dec > 0 ? dec : 0, v);
        }
        std::string out = buf;
        if (width > 0 && static_cast<int>(out.size()) > width) {
            out.resize(static_cast<std::size_t>(width));
        }
        return out;
    }

    Value call(const std::string& fn, const std::vector<Value>& args) {
        Value v;
        v.is_number = false;
        if (fn == "UPPER" && args.size() >= 1) {
            v.s = upper_utf8(args[0].s);
        } else if (fn == "LOWER" && args.size() >= 1) {
            v.s = lower_utf8(args[0].s);
        } else if (fn == "LTRIM" && args.size() >= 1) {
            v.s = ltrim_ascii(args[0].s);
        } else if (fn == "RTRIM" && args.size() >= 1) {
            v.s = rtrim_ascii(args[0].s);
        } else if (fn == "ALLTRIM" && args.size() >= 1) {
            v.s = rtrim_ascii(ltrim_ascii(args[0].s));
        } else if (fn == "DTOS" && args.size() >= 1) {
            // Date fields are already YYYYMMDD on disk; just return the raw bytes.
            v.s = args[0].s;
        } else if (fn == "STR") {
            int width = args.size() >= 2 && args[1].is_number
                      ? static_cast<int>(args[1].n) : 10;
            int dec   = args.size() >= 3 && args[2].is_number
                      ? static_cast<int>(args[2].n) : 0;
            double n = args[0].is_number ? args[0].n
                     : std::strtod(args[0].s.c_str(), nullptr);
            v.s = format_number(n, width, dec);
        } else if (fn == "SUBSTR" && args.size() >= 2) {
            const std::string& s = args[0].s;
            int start = args[1].is_number ? static_cast<int>(args[1].n) : 1;
            int len   = args.size() >= 3 && args[2].is_number
                      ? static_cast<int>(args[2].n)
                      : static_cast<int>(s.size());
            v.s = substr_utf8(s, start, len);
        } else {
            // Unknown function — return empty string.
        }
        return v;
    }

    const std::vector<Token>& toks_;
    std::size_t               pos_ = 0;
    Table&                    t_;
};

} // namespace

util::Result<std::string>
evaluate_index_expr(Table& t, const std::string& expr, std::uint16_t key_len) {
    if (expr.empty()) return std::string(key_len, ' ');

    // Fast-path: bare field-name expression (matches the M8.8 behaviour
    // exactly so existing CDX files round-trip identical bytes).
    if (std::all_of(expr.begin(), expr.end(),
            [](char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
            })) {
        // Only treat as bare-field when the identifier resolves to a
        // real column; otherwise fall through to the general parser.
        if (t.field_index(expr) >= 0) {
            std::int32_t fidx = t.field_index(expr);
            const auto& f = t.field_descriptor(static_cast<std::uint16_t>(fidx));
            auto r = t.read_field(static_cast<std::uint16_t>(fidx));
            if (!r) return r.error();
            std::string key = r.value().as_string;
            if (key.size() < key_len) key.append(key_len - key.size(), ' ');
            if (key.size() > key_len) key.resize(key_len);
            (void)f;
            return key;
        }
    }

    auto toks = tokenize(expr);
    Parser p(toks, t);
    Value v = p.parse_expr();
    std::string s = v.s;
    if (s.size() < key_len) s.append(key_len - s.size(), ' ');
    if (s.size() > key_len) s.resize(key_len);
    return s;
}

} // namespace openads::engine
