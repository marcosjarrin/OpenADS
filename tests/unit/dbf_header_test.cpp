#include "doctest.h"
#include "drivers/dbf_common.h"

#include <array>
#include <cstdint>

using openads::drivers::DbfHeader;
using openads::drivers::parse_dbf_header;

namespace {

// Minimal valid 32-byte DBF header for version 0x03 (Clipper / dBase III).
//   byte  0 : version
//   bytes 1-3 : last update YY MM DD
//   bytes 4-7 : record count (uint32 little endian)
//   bytes 8-9 : header length (uint16 little endian)
//   bytes 10-11 : record length (uint16 little endian)
//   bytes 12-31 : reserved / flags
std::array<std::uint8_t, 32> sample_header() {
    std::array<std::uint8_t, 32> h{};
    h[0]  = 0x03;
    h[1]  = 124; // year offset 2024
    h[2]  = 1;
    h[3]  = 31;
    // record count = 5
    h[4]  = 0x05; h[5] = 0; h[6] = 0; h[7] = 0;
    // header length = 0x41 (65 = 32 + 32 + 1 terminator) — single field
    h[8]  = 0x41; h[9] = 0;
    // record length = 11 (10-char field + 1 deletion byte)
    h[10] = 0x0B; h[11] = 0;
    return h;
}

} // namespace

TEST_CASE("DBF header parser extracts version, recno count, sizes") {
    auto bytes = sample_header();
    auto parsed = parse_dbf_header(bytes.data(), bytes.size());
    REQUIRE(parsed.has_value());
    DbfHeader h = parsed.value();
    CHECK(h.version       == 0x03);
    CHECK(h.record_count  == 5);
    CHECK(h.header_length == 0x41);
    CHECK(h.record_length == 0x0B);
    CHECK(h.last_update_year  == 2024);
    CHECK(h.last_update_month == 1);
    CHECK(h.last_update_day   == 31);
}

TEST_CASE("DBF header parser rejects buffers shorter than 32 bytes") {
    std::array<std::uint8_t, 16> too_small{};
    auto parsed = parse_dbf_header(too_small.data(), too_small.size());
    CHECK_FALSE(parsed.has_value());
}

TEST_CASE("DBF header parser maps version 0x30 to VFP family") {
    auto bytes = sample_header();
    bytes[0] = 0x30;
    auto parsed = parse_dbf_header(bytes.data(), bytes.size());
    REQUIRE(parsed.has_value());
    CHECK(parsed.value().family == openads::drivers::DbfFamily::Vfp);
}

TEST_CASE("DBF header parser maps version 0x03 to Clipper family") {
    auto bytes = sample_header();
    auto parsed = parse_dbf_header(bytes.data(), bytes.size());
    REQUIRE(parsed.has_value());
    CHECK(parsed.value().family == openads::drivers::DbfFamily::Clipper);
}
