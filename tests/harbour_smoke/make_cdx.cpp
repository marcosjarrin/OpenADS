// Pre-stages data.cdx for the M8.6 Harbour smoke. Uses OpenADS'
// CdxIndex::create + insert directly (no Harbour involvement) so the
// smoke validates that rddads can read a CDX produced by OpenADS.
//
// Index layout: single tag "NAME" over the NAME C(10) column with
// recno 1 -> "ALPHA", 2 -> "BETA", 3 -> "GAMMA". Order is the natural
// sort order so dbSeek lands on the expected record.

#include "drivers/cdx/cdx_index.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using openads::drivers::cdx::CdxIndex;

int main(int argc, char* argv[]) {
    fs::path out = (argc > 1)
        ? fs::path(argv[1])
        : fs::path("data.cdx");

    std::error_code ec;
    fs::remove(out, ec);

    // Tag 1: NAME (10-byte key over the C(10) NAME column).
    auto created = CdxIndex::create(out.string(), "NAME", "NAME",
                                    /*key_size*/ 10,
                                    /*unique*/   false,
                                    /*descend*/  false);
    if (!created) {
        std::fprintf(stderr, "[make_cdx] create failed: %s\n",
                     created.error().message.c_str());
        return 1;
    }
    {
        CdxIndex ix = std::move(created).value();
        struct Row { std::uint32_t recno; const char* name; };
        Row rows[] = {
            { 1, "ALPHA" },
            { 2, "BETA"  },
            { 3, "GAMMA" },
        };
        for (const Row& r : rows) {
            std::string padded = r.name;
            if (padded.size() < 10) padded.append(10 - padded.size(), ' ');
            if (auto e = ix.insert(r.recno, padded); !e) {
                std::fprintf(stderr,
                    "[make_cdx] NAME insert(%u, '%s') failed: %s\n",
                    r.recno, r.name, e.error().message.c_str());
                return 2;
            }
        }
        if (auto e = ix.flush(); !e) {
            std::fprintf(stderr, "[make_cdx] NAME flush failed: %s\n",
                         e.error().message.c_str());
            return 3;
        }
    }

    // Tag 2: AGE (3-byte key over the N(3,0) AGE column). Right-aligned
    // ASCII sorts the same way as the numeric values for the test
    // dataset (" 30" <  " 77" < "125").
    auto added = CdxIndex::add_tag(out.string(), "AGE", "AGE",
                                   /*key_size*/ 3,
                                   /*unique*/   false,
                                   /*descend*/  false);
    if (!added) {
        std::fprintf(stderr, "[make_cdx] add_tag(AGE) failed: %s\n",
                     added.error().message.c_str());
        return 4;
    }
    {
        CdxIndex ix = std::move(added).value();
        struct Row { std::uint32_t recno; const char* age; };
        Row rows[] = {
            { 1, " 30" },
            { 2, "125" },
            { 3, " 77" },
        };
        for (const Row& r : rows) {
            if (auto e = ix.insert(r.recno, r.age); !e) {
                std::fprintf(stderr,
                    "[make_cdx] AGE insert(%u, '%s') failed: %s\n",
                    r.recno, r.age, e.error().message.c_str());
                return 5;
            }
        }
        if (auto e = ix.flush(); !e) {
            std::fprintf(stderr, "[make_cdx] AGE flush failed: %s\n",
                         e.error().message.c_str());
            return 6;
        }
    }

    std::printf("[make_cdx] wrote %s with tags NAME, AGE\n", out.string().c_str());
    return 0;
}
