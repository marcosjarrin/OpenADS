#include "doctest.h"
#include "engine/table.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using openads::engine::OpenMode;
using openads::engine::Table;
using openads::engine::TableType;

namespace {

fs::path make_empty_table(const char* tag) {
    auto p = fs::temp_directory_path() / (std::string("openads_m2_w_") + tag);
    fs::remove(p);

    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 0;
    hdr[8]  = 32 + 32 + 1; hdr[9] = 0;
    hdr[10] = 1 + 5; hdr[11] = 0;
    file.insert(file.end(), hdr.begin(), hdr.end());

    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "NAME", 11);
    fd[11] = 'C';
    fd[16] = 5;
    file.insert(file.end(), fd.begin(), fd.end());
    file.push_back(0x0D);
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("Table append + set_field grows the file and round-trips") {
    auto p = make_empty_table("append");
    {
        auto t = Table::open(p.string(), TableType::Cdx, OpenMode::Shared);
        REQUIRE(t.has_value());
        Table table = std::move(t).value();

        REQUIRE(table.append_record().has_value());
        CHECK(table.recno() == 1);
        REQUIRE(table.set_field(0, std::string("Anna")).has_value());
        REQUIRE(table.flush().has_value());

        REQUIRE(table.append_record().has_value());
        CHECK(table.recno() == 2);
        REQUIRE(table.set_field(0, std::string("Bob")).has_value());
        REQUIRE(table.flush().has_value());
    }
    {
        auto t = Table::open(p.string(), TableType::Cdx, OpenMode::Read);
        REQUIRE(t.has_value());
        Table table = std::move(t).value();
        CHECK(table.record_count() == 2);

        REQUIRE(table.goto_top().has_value());
        auto v0 = table.read_field(0);
        REQUIRE(v0.has_value());
        CHECK(v0.value().as_string == "Anna");

        REQUIRE(table.skip(1).has_value());
        auto v1 = table.read_field(0);
        REQUIRE(v1.has_value());
        CHECK(v1.value().as_string == "Bob");
    }
    fs::remove(p);
}

TEST_CASE("Table mark_deleted / recall_deleted toggle the deletion byte") {
    auto p = make_empty_table("delete");
    {
        auto t = Table::open(p.string(), TableType::Cdx, OpenMode::Shared);
        REQUIRE(t.has_value());
        Table table = std::move(t).value();
        REQUIRE(table.append_record().has_value());
        REQUIRE(table.set_field(0, std::string("X")).has_value());
        REQUIRE(table.mark_deleted().has_value());
        CHECK(table.is_deleted());
        REQUIRE(table.recall_deleted().has_value());
        CHECK_FALSE(table.is_deleted());
        REQUIRE(table.flush().has_value());
    }
    fs::remove(p);
}
