#include "doctest.h"
#include "platform/file.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using openads::platform::File;
using openads::platform::OpenMode;

namespace {

fs::path tmp_path(const char* tag) {
    return fs::temp_directory_path() / (std::string("openads_test_") + tag);
}

} // namespace

TEST_CASE("File: create, write, read, delete") {
    const auto p = tmp_path("file_basic");
    fs::remove(p);

    {
        auto opened = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(opened.has_value());
        File f = std::move(opened).value();

        const std::array<std::uint8_t, 5> payload{1, 2, 3, 4, 5};
        auto wrote = f.write_at(0, payload.data(), payload.size());
        REQUIRE(wrote.has_value());
        CHECK(wrote.value() == 5);
    }

    {
        auto opened = File::open(p.string(), OpenMode::ReadOnly);
        REQUIRE(opened.has_value());
        File f = std::move(opened).value();

        std::array<std::uint8_t, 5> buf{};
        auto got = f.read_at(0, buf.data(), buf.size());
        REQUIRE(got.has_value());
        CHECK(got.value() == 5);
        CHECK(buf[0] == 1);
        CHECK(buf[4] == 5);
    }

    fs::remove(p);
}

TEST_CASE("File: opening a missing file returns AE_FILE_NOT_FOUND-like error") {
    const auto p = tmp_path("file_missing");
    fs::remove(p);
    auto opened = File::open(p.string(), OpenMode::ReadOnly);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().code != 0);
}

TEST_CASE("File: size grows with writes") {
    const auto p = tmp_path("file_size");
    fs::remove(p);
    {
        auto opened = File::open(p.string(), OpenMode::CreateRW);
        REQUIRE(opened.has_value());
        File f = std::move(opened).value();

        std::array<std::uint8_t, 8> payload{0};
        REQUIRE(f.write_at(0,  payload.data(), payload.size()).has_value());
        REQUIRE(f.write_at(16, payload.data(), payload.size()).has_value());

        auto sz = f.size();
        REQUIRE(sz.has_value());
        CHECK(sz.value() == 24);
    }
    fs::remove(p);
}
