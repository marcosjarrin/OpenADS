#include "doctest.h"
#include "openads/ace.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Stage a DBF with a 40-byte C(NOTES) column and three records carrying
// different sentences so the FTS create has something to tokenise.
fs::path stage_dbf(const fs::path& dir) {
    fs::create_directories(dir);
    auto p = dir / "data.dbf";
    fs::remove(p);
    std::vector<std::uint8_t> file;
    auto push = [&](const void* d, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(d);
        file.insert(file.end(), b, b + n);
    };
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x03;
    hdr[4]  = 3;
    hdr[8]  = 32 + 32 + 1;
    hdr[10] = 1 + 40;
    push(hdr.data(), hdr.size());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "NOTES", 11);
    fd[11] = 'C'; fd[16] = 40;
    push(fd.data(), fd.size());
    file.push_back(0x0D);

    auto rec = [&](const char* s) {
        file.push_back(' ');
        for (int i = 0; i < 40; ++i)
            file.push_back(i < (int)std::strlen(s)
                           ? static_cast<std::uint8_t>(s[i]) : ' ');
    };
    rec("Quick brown fox jumps over the lazy dog");
    rec("The lazy dog sleeps in the sun");
    rec("Quick fox returns to the den at dusk");
    file.push_back(0x1A);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream os; os << f.rdbuf();
    return os.str();
}

}  // namespace

TEST_CASE("M9.19 AdsCreateFTSIndex emits an inverted-index file") {
    const auto dir = fs::temp_directory_path() / "openads_m9_19_fts";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    UNSIGNED8 leaf[16] = "data";
    ADSHANDLE hTable = 0;
    REQUIRE(AdsOpenTable(hConn, leaf, leaf, ADS_CDX,
                         1, 1, 0, 1, &hTable) == 0);

    UNSIGNED8 fts_path[64] = "notes.fts";
    UNSIGNED8 tag[16]      = "NOTESFTS";
    UNSIGNED8 field[16]    = "NOTES";
    UNSIGNED8 nullbuf[2]   = {0, 0};

    UNSIGNED32 rc = AdsCreateFTSIndex(
        hTable, fts_path, tag, field,
        /*ulPageSize=*/0,
        /*ulMinWordLen=*/3,
        /*ulMaxWordLen=*/30,
        /*usUseDefaultDelim=*/1, nullbuf,
        /*usUseDefaultNoise=*/1, nullbuf,
        /*usUseDefaultDrop=*/1,  nullbuf,
        /*usUseDefaultConditionals=*/1, nullbuf,
        nullbuf, nullbuf,
        /*ulOptions=*/0);
    REQUIRE(rc == 0);

    auto out_path = dir / "notes.fts";
    REQUIRE(fs::exists(out_path));
    auto contents = read_file(out_path);
    CHECK(contents.find("# OpenADS FTS v0") == 0);
    CHECK(contents.find("tag=NOTESFTS") != std::string::npos);
    CHECK(contents.find("field=NOTES")  != std::string::npos);

    // Token "quick" appears in records 1 and 3.
    CHECK(contents.find("\nquick\t1,3") != std::string::npos);
    // Token "fox" appears in records 1 and 3.
    CHECK(contents.find("\nfox\t1,3") != std::string::npos);
    // "the" is in default noise list, must NOT be emitted.
    CHECK(contents.find("\nthe\t") == std::string::npos);
    // "dog" appears in 1 and 2.
    CHECK(contents.find("\ndog\t1,2") != std::string::npos);
    // "lazy" in 1 and 2.
    CHECK(contents.find("\nlazy\t1,2") != std::string::npos);
    // "sun" only in record 2.
    CHECK(contents.find("\nsun\t2\n") != std::string::npos);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.19 AdsCreateFTSIndex with NULL pucFileName uses table-stem.fts") {
    const auto dir = fs::temp_directory_path() / "openads_m9_19_fts_auto";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    UNSIGNED8 leaf[16] = "data";
    ADSHANDLE hTable = 0;
    REQUIRE(AdsOpenTable(hConn, leaf, leaf, ADS_CDX,
                         1, 1, 0, 1, &hTable) == 0);

    UNSIGNED8 tag[16]    = "AUTO";
    UNSIGNED8 field[16]  = "NOTES";
    UNSIGNED8 nullbuf[2] = {0, 0};
    UNSIGNED32 rc = AdsCreateFTSIndex(
        hTable, /*pucFileName=*/nullptr, tag, field,
        0, 3, 30,
        1, nullbuf, 1, nullbuf, 1, nullbuf, 1, nullbuf,
        nullbuf, nullbuf, 0);
    REQUIRE(rc == 0);
    CHECK(fs::exists(dir / "data.fts"));

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}

TEST_CASE("M9.19 AdsCreateFTSIndex on missing field returns error") {
    const auto dir = fs::temp_directory_path() / "openads_m9_19_fts_miss";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);
    UNSIGNED8 leaf[16] = "data";
    ADSHANDLE hTable = 0;
    REQUIRE(AdsOpenTable(hConn, leaf, leaf, ADS_CDX,
                         1, 1, 0, 1, &hTable) == 0);

    UNSIGNED8 fts_path[64] = "miss.fts";
    UNSIGNED8 tag[16]      = "MISS";
    UNSIGNED8 field[16]    = "NOSUCH";
    UNSIGNED8 nullbuf[2]   = {0, 0};
    UNSIGNED32 rc = AdsCreateFTSIndex(
        hTable, fts_path, tag, field,
        0, 3, 30,
        1, nullbuf, 1, nullbuf, 1, nullbuf, 1, nullbuf,
        nullbuf, nullbuf, 0);
    CHECK(rc != 0);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
