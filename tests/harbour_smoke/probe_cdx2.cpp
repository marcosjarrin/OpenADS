#include "drivers/cdx/cdx_index.h"
#include <cstdint>
#include <cstdio>
#include <fstream>

using openads::drivers::cdx::CdxIndex;
using openads::drivers::IndexOpenMode;

static std::uint32_t rd32_le(const std::uint8_t* p) {
    return  std::uint32_t(p[0])        |
           (std::uint32_t(p[1]) <<  8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}
static std::uint16_t rd16_le(const std::uint8_t* p) {
    return  std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: probe_cdx2 <path.cdx> <tag>\n");
        return 1;
    }
    std::ifstream f(argv[1], std::ios::binary | std::ios::ate);
    auto fsize = f.tellg();
    std::printf("file size=%lld\n", static_cast<long long>(fsize));
    f.seekg(0);
    std::uint8_t fh[1024]{};
    f.read(reinterpret_cast<char*>(fh), 1024);
    std::uint32_t struct_root = rd32_le(fh + 0);
    std::printf("struct_root=%u\n", struct_root);
    f.seekg(struct_root);
    std::uint8_t leaf[512]{};
    f.read(reinterpret_cast<char*>(leaf), 512);
    std::uint16_t nkeys = rd16_le(leaf + 2);
    std::printf("struct leaf attr=%u nkeys=%u\n",
                rd16_le(leaf + 0), nkeys);

    // Decode the struct-tag leaf (key_size = 10) — recno field is the
    // sub-tag CDXTAGHEADER offset in this compound index.
    auto dec = openads::drivers::cdx::CdxIndex::list_tags(argv[1]);
    if (!dec) {
        std::fprintf(stderr, "list_tags: %s\n", dec.error().message.c_str());
        return 1;
    }
    for (auto& t : dec.value()) {
        std::printf("  tag '%s'\n", t.c_str());
    }

    // Show the first 64 bytes of each sub-tag header offset.
    for (std::uint16_t i = 0; i < nkeys; ++i) {
        // sub-tag offset is in entry's recno field. Use the public
        // open_named to fetch it indirectly via key_size after the
        // header is decoded.
    }

    // Decode the struct leaf into name → header_off pairs.
    {
        std::uint16_t b_bits = 0;
        for (std::uint16_t v = 10; v; v >>= 1) ++b_bits;
        std::uint8_t key_bytes = (b_bits > 12) ? 5 : (b_bits > 8 ? 4 : 3);
        std::uint8_t rec_bits = static_cast<std::uint8_t>(
            (key_bytes << 3) - (b_bits << 1));
        std::uint32_t rec_mask = (rec_bits >= 32)
            ? 0xFFFFFFFFu : ((1u << rec_bits) - 1);
        std::printf("struct: key_bytes=%u rec_bits=%u rec_mask=%08x\n",
                    key_bytes, rec_bits, rec_mask);
        for (std::uint16_t i = 0; i < nkeys; ++i) {
            const std::uint8_t* e = leaf + 24 + i * key_bytes;
            std::uint32_t recno = rd32_le(e) & rec_mask;
            std::printf("  entry %u: recno=%u (header_off?)\n", i, recno);
        }
    }

    // For the requested tag, try open_named and print structural info
    // even if seek_first fails.
    CdxIndex ix;
    auto orn = ix.open_named(argv[1], IndexOpenMode::Shared, argv[2]);
    if (!orn) {
        std::fprintf(stderr, "open_named: %s\n", orn.error().message.c_str());
        return 1;
    }
    std::printf("tag=%s key_size=%u expr=%s\n",
                ix.name().c_str(), unsigned(ix.key_length()),
                ix.expression().c_str());
    return 0;
}
