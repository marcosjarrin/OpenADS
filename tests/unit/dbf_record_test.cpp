#include "doctest.h"
#include "drivers/dbf_common.h"

#include <cstdint>
#include <string>
#include <vector>

using openads::drivers::DbfField;
using openads::drivers::DbfFieldType;
using openads::drivers::decode_field;
using openads::drivers::record_is_deleted;

TEST_CASE("Character field decodes ASCII trimmed of trailing spaces") {
    DbfField f;
    f.type          = DbfFieldType::Character;
    f.length        = 10;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        'C', 'l', 'i', 'p', 'p', 'e', 'r', ' ', ' ', ' '};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_string == "Clipper");
}

TEST_CASE("Numeric field decodes an integer-looking value") {
    DbfField f;
    f.type          = DbfFieldType::Numeric;
    f.length        = 6;
    f.decimals      = 0;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        ' ', ' ', ' ', '4', '2', '0'};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_double == doctest::Approx(420.0));
}

TEST_CASE("Numeric field decodes a fractional value") {
    DbfField f;
    f.type          = DbfFieldType::Numeric;
    f.length        = 7;
    f.decimals      = 2;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        ' ', ' ', '3', '.', '1', '4', ' '};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_double == doctest::Approx(3.14));
}

TEST_CASE("Logical field decodes T/F/Y/N") {
    DbfField f;
    f.type          = DbfFieldType::Logical;
    f.length        = 1;
    f.record_offset = 1;
    for (auto [byte, expected] : std::initializer_list<std::pair<char, bool>>{
             {'T', true}, {'t', true}, {'Y', true}, {'y', true},
             {'F', false}, {'f', false}, {'N', false}, {'n', false},
             {'?', false}, {' ', false}}) {
        std::vector<std::uint8_t> rec = {' ',
            static_cast<std::uint8_t>(byte)};
        auto v = decode_field(f, rec.data(), rec.size());
        REQUIRE(v.has_value());
        CHECK(v.value().as_bool == expected);
    }
}

TEST_CASE("Date field decodes YYYYMMDD ASCII") {
    DbfField f;
    f.type          = DbfFieldType::Date;
    f.length        = 8;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        '2', '0', '2', '6', '0', '5', '0', '3'};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_string == "20260503");
}

TEST_CASE("Memo field returns an empty placeholder in M1") {
    DbfField f;
    f.type          = DbfFieldType::Memo;
    f.length        = 10;
    f.record_offset = 1;
    std::vector<std::uint8_t> rec = {' ',
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    auto v = decode_field(f, rec.data(), rec.size());
    REQUIRE(v.has_value());
    CHECK(v.value().as_string.empty());
}

TEST_CASE("record_is_deleted reads the deletion byte") {
    std::vector<std::uint8_t> alive   = {' ', 'a', 'b', 'c'};
    std::vector<std::uint8_t> deleted = {'*', 'a', 'b', 'c'};
    CHECK_FALSE(record_is_deleted(alive.data(),   alive.size()));
    CHECK      (record_is_deleted(deleted.data(), deleted.size()));
}
