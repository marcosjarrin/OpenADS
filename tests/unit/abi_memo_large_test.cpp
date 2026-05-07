#include "doctest.h"
#include "drivers/fpt/fpt_memo.h"
#include "openads/ace.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path build_memo_dbf(const fs::path& dir) {
    fs::create_directories(dir);
    auto p = dir / "big.dbf";
    fs::remove(p);

    constexpr int field_count = 2;
    std::uint16_t header_len = static_cast<std::uint16_t>(
        32 + 32 * field_count + 1);
    std::uint16_t rec_len = 1 + 4 + 10;

    std::vector<std::uint8_t> file;
    std::array<std::uint8_t, 32> hdr{};
    hdr[0]  = 0x83;
    hdr[8]  = static_cast<std::uint8_t>(header_len & 0xFFu);
    hdr[9]  = static_cast<std::uint8_t>((header_len >> 8) & 0xFFu);
    hdr[10] = static_cast<std::uint8_t>(rec_len & 0xFFu);
    hdr[11] = static_cast<std::uint8_t>((rec_len >> 8) & 0xFFu);
    file.insert(file.end(), hdr.begin(), hdr.end());

    auto add = [&](const char* name, char type, std::uint8_t len) {
        std::array<std::uint8_t, 32> fd{};
        std::strncpy(reinterpret_cast<char*>(fd.data()), name, 11);
        fd[11] = static_cast<std::uint8_t>(type);
        fd[16] = len;
        file.insert(file.end(), fd.begin(), fd.end());
    };
    add("ID",   'C', 4);
    add("BLOB", 'M', 10);
    file.push_back(0x0D);
    file.push_back(0x1A);

    std::ofstream(p, std::ios::binary).write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    return p;
}

} // namespace

// AdsGetField used to cap memo reads at 65 534 bytes by truncating
// the in/out length to UNSIGNED16, even though the SAP signature
// passes a UNSIGNED32*. This pins the bug so it doesn't regress.
TEST_CASE("AdsGetField returns memos larger than 64 KB intact") {
    auto dir = fs::temp_directory_path() / "openads_memo_large";
    std::error_code ec;
    fs::remove_all(dir, ec);

    build_memo_dbf(dir);
    auto fpt_path = dir / "big.fpt";
    REQUIRE(openads::drivers::fpt::FptMemo::create(fpt_path.string(), 64)
            .has_value());

    UNSIGNED8 srv[256];
    std::memcpy(srv, dir.string().c_str(), dir.string().size() + 1);
    ADSHANDLE hConn = 0;
    REQUIRE(AdsConnect60(srv, ADS_LOCAL_SERVER,
                         nullptr, nullptr, 0, &hConn) == 0);

    UNSIGNED8 leaf[64] = "big.dbf";
    ADSHANDLE hTable = 0;
    REQUIRE(AdsOpenTable(hConn, leaf, leaf, ADS_CDX,
                         0, 0, 0, 0, &hTable) == 0);

    REQUIRE(AdsAppendRecord(hTable) == 0);

    constexpr UNSIGNED32 N = 200000;          // > 64 KB by 3x
    std::vector<UNSIGNED8> payload(N);
    for (UNSIGNED32 i = 0; i < N; ++i) {
        payload[i] = static_cast<UNSIGNED8>((i * 31u + 7u) & 0xFFu);
    }
    UNSIGNED8 fblob[16] = "BLOB";
    REQUIRE(AdsSetString(hTable, fblob, payload.data(), N) == 0);
    REQUIRE(AdsWriteRecord(hTable) == 0);

    UNSIGNED32 mlen = 0;
    REQUIRE(AdsGetMemoLength(hTable, fblob, &mlen) == 0);
    REQUIRE(mlen == N);

    std::vector<UNSIGNED8> rd(mlen + 1, 0);
    UNSIGNED32 rlen = mlen + 1;
    REQUIRE(AdsGetField(hTable, fblob, rd.data(), &rlen, 0) == 0);
    CHECK(rlen == N);

    bool match = true;
    for (UNSIGNED32 i = 0; i < N; ++i) {
        if (rd[i] != payload[i]) { match = false; break; }
    }
    CHECK(match);

    REQUIRE(AdsCloseTable(hTable) == 0);
    REQUIRE(AdsDisconnect(hConn) == 0);
    fs::remove_all(dir, ec);
}
