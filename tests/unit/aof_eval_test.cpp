// M-AOF.2 — full-scan AST → bitmap evaluator. Builds a tiny DBF on
// disk with a character + numeric + logical field, parses an AOF
// expression, and asserts the produced bitmap matches the expected
// per-record verdict.

#include "doctest.h"
#include "engine/aof_eval.h"
#include "engine/aof_expr.h"
#include "engine/table.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using openads::engine::Table;
using openads::engine::TableType;
using openads::engine::aof::Bitmap;

namespace {

// Two-field, four-row DBF:
//   NAME  C(5)
//   AGE   N(3,0)
//
// Rows:
//   1  AAA   25
//   2  BBB   42
//   3  CCC   30
//   4  DDD   18
fs::path make_fixture(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("openads_aof_") + tag);
    fs::remove(p);

    std::vector<std::uint8_t> file;

    constexpr std::uint16_t header_size = 32 + 32 + 32 + 1;   // 2 fields
    constexpr std::uint16_t record_size = 1 + 5 + 3;          // del + NAME + AGE

    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[1]  = 124; hdr[2] = 1; hdr[3] = 31;
    hdr[4]  = 4; hdr[5] = 0; hdr[6] = 0; hdr[7] = 0;
    hdr[8]  = static_cast<std::uint8_t>(header_size & 0xFF);
    hdr[9]  = static_cast<std::uint8_t>((header_size >> 8) & 0xFF);
    hdr[10] = static_cast<std::uint8_t>(record_size & 0xFF);
    hdr[11] = static_cast<std::uint8_t>((record_size >> 8) & 0xFF);
    file.insert(file.end(), hdr.begin(), hdr.end());

    auto push_field = [&](const char* name, char type,
                          std::uint8_t length, std::uint8_t dec) {
        std::array<std::uint8_t, 32> fd{};
        std::strncpy(reinterpret_cast<char*>(fd.data()), name, 11);
        fd[11] = static_cast<std::uint8_t>(type);
        fd[16] = length;
        fd[17] = dec;
        file.insert(file.end(), fd.begin(), fd.end());
    };
    push_field("NAME", 'C', 5, 0);
    push_field("AGE",  'N', 3, 0);
    file.push_back(0x0D);

    auto push_rec = [&](const char* name, const char* age) {
        file.push_back(' ');
        for (int i = 0; i < 5; ++i) {
            file.push_back(static_cast<std::uint8_t>(
                i < static_cast<int>(std::strlen(name)) ? name[i] : ' '));
        }
        // Numeric: right-aligned, space-padded.
        std::string a(age);
        while (a.size() < 3) a.insert(a.begin(), ' ');
        for (char c : a) file.push_back(static_cast<std::uint8_t>(c));
    };
    push_rec("AAA", "25");
    push_rec("BBB", "42");
    push_rec("CCC", "30");
    push_rec("DDD", "18");
    file.push_back(0x1A);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

Bitmap eval(const std::string& src, Table& t) {
    auto ast = openads::engine::aof::parse(src);
    REQUIRE(ast);
    auto bm = openads::engine::aof::evaluate(*ast.value(), t);
    REQUIRE(bm);
    return std::move(bm).value();
}

} // namespace

TEST_CASE("aof_eval: equality on a numeric field") {
    auto p = make_fixture("eq_num");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval("AGE = 30", t);
        REQUIRE(bm.size() == 4);
        CHECK_FALSE(bm[0]);  // 25
        CHECK_FALSE(bm[1]);  // 42
        CHECK     (bm[2]);   // 30
        CHECK_FALSE(bm[3]);  // 18
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: equality on a character field") {
    auto p = make_fixture("eq_char");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval("NAME = 'BBB'", t);
        REQUIRE(bm.size() == 4);
        CHECK_FALSE(bm[0]);
        CHECK     (bm[1]);
        CHECK_FALSE(bm[2]);
        CHECK_FALSE(bm[3]);
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: BETWEEN inclusive on numeric") {
    auto p = make_fixture("between");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval("AGE BETWEEN 20 AND 35", t);
        REQUIRE(bm.size() == 4);
        CHECK     (bm[0]);   // 25
        CHECK_FALSE(bm[1]);  // 42
        CHECK     (bm[2]);   // 30
        CHECK_FALSE(bm[3]);  // 18
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: IN list on character") {
    auto p = make_fixture("in_char");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval("NAME IN ('AAA','CCC')", t);
        REQUIRE(bm.size() == 4);
        CHECK     (bm[0]);
        CHECK_FALSE(bm[1]);
        CHECK     (bm[2]);
        CHECK_FALSE(bm[3]);
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: AND combiner") {
    auto p = make_fixture("and");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        // age >= 25 AND name <= 'BBB'  →  recnos 1, 2 only.
        auto bm = eval("AGE >= 25 .AND. NAME <= 'BBB'", t);
        REQUIRE(bm.size() == 4);
        CHECK     (bm[0]);
        CHECK     (bm[1]);
        CHECK_FALSE(bm[2]);
        CHECK_FALSE(bm[3]);
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: OR combiner") {
    auto p = make_fixture("or");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval("AGE = 18 OR AGE = 42", t);
        REQUIRE(bm.size() == 4);
        CHECK_FALSE(bm[0]);
        CHECK     (bm[1]);
        CHECK_FALSE(bm[2]);
        CHECK     (bm[3]);
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: NOT combiner") {
    auto p = make_fixture("not");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval(".NOT. AGE >= 30", t);
        REQUIRE(bm.size() == 4);
        CHECK     (bm[0]);   // 25
        CHECK_FALSE(bm[1]);  // 42
        CHECK_FALSE(bm[2]);  // 30
        CHECK     (bm[3]);   // 18
    }
    fs::remove(p);
}

TEST_CASE("aof_eval: unknown field never matches") {
    auto p = make_fixture("unk_field");
    {
        auto opened = Table::open(p.string(), TableType::Cdx);
        REQUIRE(opened);
        Table t = std::move(opened).value();
        auto bm = eval("NOT_A_FIELD = 1", t);
        REQUIRE(bm.size() == 4);
        for (bool b : bm) CHECK_FALSE(b);
    }
    fs::remove(p);
}
