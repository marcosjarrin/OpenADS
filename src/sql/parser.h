#pragma once

#include "util/result.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openads::sql {

// SELECT * FROM <table> [WHERE <expr>]
//
// Where <expr> is either a column comparison (`<col> op <lit>` —
// six infix operators), a CONTAINS(<col>, '<lit>') FTS predicate,
// or a boolean tree of those built from AND / OR / NOT / parens.
// String and numeric literals are supported. Projection lists,
// joins, aggregates, subqueries, ORDER BY, INSERT / UPDATE / DELETE
// land in subsequent milestones.

enum class WhereOp { Eq, Ne, Lt, Gt, Le, Ge, Contains };

struct WhereCmp {
    std::string column;
    WhereOp     op = WhereOp::Eq;
    std::string literal;       // raw string content, unquoted
    bool        is_numeric = false;
    double      number     = 0.0;
};

struct WhereExpr {
    // Tagged tree node.
    enum class Kind { Cmp, And, Or, Not };
    Kind                       kind = Kind::Cmp;
    WhereCmp                   cmp;             // Kind::Cmp
    std::vector<std::unique_ptr<WhereExpr>> children; // And/Or
    std::unique_ptr<WhereExpr> child;           // Not
};

struct SelectStmt {
    std::string                table;
    // Optional WHERE — tree form. nullptr means "no filter".
    std::unique_ptr<WhereExpr> where;
};

util::Result<SelectStmt> parse_select(const std::string& sql);

} // namespace openads::sql
