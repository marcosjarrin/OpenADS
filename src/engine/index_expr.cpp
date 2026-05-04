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

std::string upper_ascii(const std::string& s) {
    std::string out = s;
    for (auto& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string lower_ascii(const std::string& s) {
    std::string out = s;
    for (auto& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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
                return call(upper_ascii(name), args);
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
            v.s = upper_ascii(args[0].s);
        } else if (fn == "LOWER" && args.size() >= 1) {
            v.s = lower_ascii(args[0].s);
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
            if (start < 1) start = 1;
            std::size_t off = static_cast<std::size_t>(start - 1);
            if (off >= s.size()) return v;
            v.s = s.substr(off, static_cast<std::size_t>(std::max(0, len)));
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
