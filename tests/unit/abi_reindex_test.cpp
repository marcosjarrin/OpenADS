#include "doctest.h"
#include "openads/ace.h"

#include "drivers/cdx/cdx_index.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using openads::drivers::cdx::CdxIndex;

namespace {

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
    hdr[0]  = 0x03; hdr[4]  = 3;
    hdr[8]  = 32 + 32 + 1; hdr[10] = 1 + 4;
    push(hdr.data(), hdr.size());
    std::array<std::uint8_t, 32> fd{};
    std::strncpy(reinterpret_cast<char*>(fd.data()), "TAG", 11);
    fd[11] = 'C'; fd[16] = 4;
    push(fd.data(), fd.size());
    file.push_back(0x0D);
    auto rec = [&](const char* s) {
        file.push_back(' ');
        for (int i = 0; i < 4; ++i)
            file.push_back(i < (int)std::strlen(s)
                           ? static_cast<std::uint8_t>(s[i]) : ' ');
    };
    rec("BBBB"); rec("AAAA"); rec("CCCC");
    file.push_back(0x1A);
    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

TEST_CASE("M9.9 AdsReindex rebuilds active CDX entries from current records") {
    auto dir = fs::temp_directory_path() / "openads_m99_reindex";
    std::error_code ec;
    fs::remove_all(dir, ec);
    stage_dbf(dir);

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    ADSHANDLE hTable = 0;
    UNSIGNED8 name[16] = "data";
    REQUIRE(AdsOpenTable(hConn, name, name, ADS_CDX, 1, 1, 0, 1, &hTable) == 0);

    UNSIGNED8 idxfile[16] = "data";
    UNSIGNED8 idxname[16] = "TAG";
    UNSIGNED8 expr[16]    = "TAG";
    ADSHANDLE hIdx = 0;
    REQUIRE(AdsCreateIndex61(hTable, idxfile, idxname, expr,
                             nullptr, nullptr,
                             0, 512, &hIdx) == 0);

    // Sanity: walk in TAG order before reindex.
    REQUIRE(AdsGotoTop(hTable) == 0);
    UNSIGNED32 recno = 0;
    REQUIRE(AdsGetRecordNum(hTable, 0, &recno) == 0);
    CHECK(recno == 2);  // AAAA

    // Mutate record 2 directly via fstream so the index is now stale.
    {
        std::fstream f(dir / "data.dbf",
                       std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(65 + (2 - 1) * 5 + 1);  // header_len + (rec-1)*rec_len + del-flag
        f.write("ZZZZ", 4);
    }

    // Refresh the in-memory record buffer for recno 2 so the engine
    // sees the new bytes; AdsReindex reads through goto_record so a
    // refresh isn't strictly required, but matches a real app's flow.
    REQUIRE(AdsGotoRecord(hTable, 2) == 0);
    REQUIRE(AdsRefreshRecord(hTable) == 0);

    // Reindex from current records.
    REQUIRE(AdsReindex(hTable) == 0);

    // After reindex, AdsSeek('AAAA') must miss (the record is now
    // ZZZZ); AdsSeek('ZZZZ') must hit recno 2.
    UNSIGNED8 key1[8] = "AAAA";
    UNSIGNED16 found  = 0;
    REQUIRE(AdsSeek(hIdx, key1, 4, 0, 0, &found) == 0);
    CHECK(found == 0);

    UNSIGNED8 key2[8] = "ZZZZ";
    found = 0;
    REQUIRE(AdsSeek(hIdx, key2, 4, 0, 0, &found) == 0);
    CHECK(found == 1);
    REQUIRE(AdsGetRecordNum(hTable, 0, &recno) == 0);
    CHECK(recno == 2);

    REQUIRE(AdsCloseIndex(hIdx) == 0);
    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
