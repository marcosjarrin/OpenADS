#include "doctest.h"
#include "drivers/dbf_common.h"

#include <cstdint>
#include <string>
#include <vector>

using openads::drivers::DbfField;
using openads::drivers::DbfFieldType;
using openads::drivers::encode_field_string;
using openads::drivers::encode_field_double;
using openads::drivers::encode_field_logical;
using openads::drivers::make_empty_record;
using openads::drivers::set_record_deleted;

TEST_CASE("make_empty_record fills a buffer with the deletion byte and spaces") {
    std::uint16_t rec_len = 1 + 4 + 3;
    auto rec = make_empty_record(rec_len);
    REQUIRE(rec.size() == rec_len);
    CHECK(rec[0] == ' ');
    for (std::size_t i = 1; i < rec.size(); ++i) CHECK(rec[i] == ' ');
}

TEST_CASE("encode_field_string left-justifies and pads with spaces") {
    DbfField f;
    f.type = DbfFieldType::Character;
    f.length = 5;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 5);
    auto r = encode_field_string(f, rec.data(), rec.size(), "hi");
    REQUIRE(r.has_value());
    CHECK(rec[1] == 'h');
    CHECK(rec[2] == 'i');
    CHECK(rec[3] == ' ');
    CHECK(rec[4] == ' ');
    CHECK(rec[5] == ' ');
}

TEST_CASE("encode_field_string truncates oversized input to field length") {
    DbfField f;
    f.type = DbfFieldType::Character;
    f.length = 3;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 3);
    auto r = encode_field_string(f, rec.data(), rec.size(), "abcdef");
    REQUIRE(r.has_value());
    CHECK(rec[1] == 'a');
    CHECK(rec[2] == 'b');
    CHECK(rec[3] == 'c');
}

TEST_CASE("encode_field_double right-justifies a numeric field with decimals") {
    DbfField f;
    f.type = DbfFieldType::Numeric;
    f.length = 7;
    f.decimals = 2;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 7);
    auto r = encode_field_double(f, rec.data(), rec.size(), 3.14);
    REQUIRE(r.has_value());
    std::string s(reinterpret_cast<const char*>(rec.data() + 1), 7);
    CHECK(s == "   3.14");
}

TEST_CASE("encode_field_logical writes T or F") {
    DbfField f;
    f.type = DbfFieldType::Logical;
    f.length = 1;
    f.record_offset = 1;
    auto rec = make_empty_record(1 + 1);
    REQUIRE(encode_field_logical(f, rec.data(), rec.size(), true).has_value());
    CHECK(rec[1] == 'T');
    REQUIRE(encode_field_logical(f, rec.data(), rec.size(), false).has_value());
    CHECK(rec[1] == 'F');
}

TEST_CASE("set_record_deleted toggles the deletion byte") {
    auto rec = make_empty_record(4);
    set_record_deleted(rec.data(), rec.size(), true);
    CHECK(rec[0] == '*');
    set_record_deleted(rec.data(), rec.size(), false);
    CHECK(rec[0] == ' ');
}
