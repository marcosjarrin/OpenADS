// M-AOF.2 — evaluator that turns an AOF AST into a per-record
// bitmap. The V1 path is a full table scan: for every recno in
// [1, record_count()] decode the relevant fields once, evaluate the
// AST, and set the bit if the AST returns true. M-AOF.4 will short-
// circuit individual leaves through CDX/NTX index range scans
// without changing this file's public contract.

#include "engine/aof_eval.h"
#include "engine/table.h"
#include "drivers/dbf_common.h"
#include "drivers/driver_trait.h"
#include "openads/error.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace openads::engine::aof {

namespace {

// Trim trailing ASCII spaces. CDX-style character indexes are
// right-padded; raw DBF reads come back left-aligned with trailing
// spaces. AOF leaves operate on the logical value, so we strip
// padding before comparing.
std::string rtrim(std::string s) {
    while (!s.empty() &&
           static_cast<unsigned char>(s.back()) <= ' ') {
        s.pop_back();
    }
    return s;
}

// Lowercase a string for case-insensitive field name matching.
std::string lower(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Compare a decoded field value against a literal Value. Returns -1
// / 0 / +1 like strcmp. Coerces both sides into the field's
// "natural" domain (string for C/M, double for N/F/I/B/Y/D, bool
// for L). String comparisons are byte-lexicographic so that they
// match the CDX / NTX key ordering once we route this through the
// index in M-AOF.4.
int cmp_field_to_literal(const drivers::DbfField&     fld,
                         const drivers::DbfFieldValue& v,
                         const Value& lit) {
    using drivers::DbfFieldType;
    auto numeric_lit = [&]() -> double {
        if (auto p = std::get_if<std::int64_t>(&lit)) return static_cast<double>(*p);
        if (auto p = std::get_if<double>(&lit))      return *p;
        if (auto p = std::get_if<std::string>(&lit)) {
            try { return std::stod(*p); } catch (...) { return 0.0; }
        }
        return 0.0;
    };
    auto string_lit = [&]() -> std::string {
        if (auto p = std::get_if<std::string>(&lit)) return *p;
        if (auto p = std::get_if<std::int64_t>(&lit)) return std::to_string(*p);
        if (auto p = std::get_if<double>(&lit))      return std::to_string(*p);
        return {};
    };

    switch (fld.type) {
        case DbfFieldType::Character:
        case DbfFieldType::Memo:
        case DbfFieldType::Varchar:
        case DbfFieldType::Date:
        case DbfFieldType::DateTime: {
            std::string lhs = rtrim(v.as_string);
            std::string rhs = string_lit();
            if (lhs < rhs) return -1;
            if (lhs > rhs) return  1;
            return 0;
        }
        case DbfFieldType::Numeric:
        case DbfFieldType::Float:
        case DbfFieldType::Integer:
        case DbfFieldType::Currency:
        case DbfFieldType::Double: {
            double lhs = v.as_double;
            double rhs = numeric_lit();
            if (lhs < rhs) return -1;
            if (lhs > rhs) return  1;
            return 0;
        }
        case DbfFieldType::Logical: {
            bool lhs = v.as_bool;
            bool rhs = false;
            if (auto pi = std::get_if<std::int64_t>(&lit)) rhs = (*pi != 0);
            else if (auto pd = std::get_if<double>(&lit))  rhs = (*pd != 0.0);
            else if (auto ps = std::get_if<std::string>(&lit)) {
                if (!ps->empty()) {
                    char c = static_cast<char>(
                        std::toupper(static_cast<unsigned char>((*ps)[0])));
                    rhs = (c == 'T' || c == 'Y' || c == '1');
                }
            }
            if (lhs == rhs) return 0;
            return lhs ? 1 : -1;
        }
        default:
            return 0;
    }
}

bool eval_leaf(const Leaf& leaf, Table& t) {
    auto idx = t.field_index(leaf.field);
    if (idx < 0) return false;                  // unknown field → never match
    auto val = t.read_field(static_cast<std::uint16_t>(idx));
    if (!val) return false;
    const auto& fld = t.driver()->fields().at(static_cast<std::size_t>(idx));
    const drivers::DbfFieldValue& v = val.value();

    switch (leaf.op) {
        case Op::Eq: return cmp_field_to_literal(fld, v, leaf.values[0]) == 0;
        case Op::Ne: return cmp_field_to_literal(fld, v, leaf.values[0]) != 0;
        case Op::Lt: return cmp_field_to_literal(fld, v, leaf.values[0]) <  0;
        case Op::Le: return cmp_field_to_literal(fld, v, leaf.values[0]) <= 0;
        case Op::Gt: return cmp_field_to_literal(fld, v, leaf.values[0]) >  0;
        case Op::Ge: return cmp_field_to_literal(fld, v, leaf.values[0]) >= 0;
        case Op::Between:
            return cmp_field_to_literal(fld, v, leaf.values[0]) >= 0 &&
                   cmp_field_to_literal(fld, v, leaf.values[1]) <= 0;
        case Op::In:
            for (auto& lit : leaf.values) {
                if (cmp_field_to_literal(fld, v, lit) == 0) return true;
            }
            return false;
    }
    return false;
}

bool eval_node(const Node& n, Table& t) {
    if (auto p = std::get_if<Leaf>(&n.v)) return eval_leaf(*p, t);
    if (auto p = std::get_if<And>(&n.v)) {
        for (auto& k : p->kids) if (!eval_node(*k, t)) return false;
        return true;
    }
    if (auto p = std::get_if<Or>(&n.v)) {
        for (auto& k : p->kids) if (eval_node(*k, t)) return true;
        return false;
    }
    if (auto p = std::get_if<Not>(&n.v)) {
        return !eval_node(*p->child, t);
    }
    return false;
}

} // namespace

util::Result<Bitmap> evaluate(const Node& n, Table& t) {
    auto rc = t.record_count();
    Bitmap bm;
    bm.assign(rc, false);
    for (std::uint32_t r = 1; r <= rc; ++r) {
        auto g = t.goto_record(r);
        if (!g) return g.error();
        bm[r - 1] = eval_node(n, t);
    }
    return bm;
}

} // namespace openads::engine::aof
