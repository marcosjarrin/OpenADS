// Pre-stages data.cdx (and, since M8.11, an empty data.fpt) for the
// Harbour smoke. Uses OpenADS' CdxIndex::create / FptMemo::create
// directly so the smoke validates that rddads can read OpenADS-built
// fixtures.

#include "drivers/cdx/cdx_index.h"
#include "drivers/fpt/fpt_memo.h"

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

    // Tag 3: UPPER_NAME with key expression UPPER(NAME). Existing
    // records already store NAME upper-cased; the keys here are the
    // same bytes as the NAME tag's. The point is that subsequent
    // mutations (dbAppend in the smoke) must run through OpenADS'
    // M9.1 compound-expression evaluator to keep this tag in sync,
    // and rddads must be able to seek through it via OrdSetFocus.
    auto added2 = CdxIndex::add_tag(out.string(), "UPPER_NAME",
                                    "UPPER(NAME)",
                                    /*key_size*/ 10,
                                    /*unique*/   false,
                                    /*descend*/  false);
    if (!added2) {
        std::fprintf(stderr, "[make_cdx] add_tag(UPPER_NAME) failed: %s\n",
                     added2.error().message.c_str());
        return 8;
    }
    {
        CdxIndex ix = std::move(added2).value();
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
                    "[make_cdx] UPPER_NAME insert(%u, '%s') failed: %s\n",
                    r.recno, r.name, e.error().message.c_str());
                return 9;
            }
        }
        if (auto e = ix.flush(); !e) {
            std::fprintf(stderr, "[make_cdx] UPPER_NAME flush failed: %s\n",
                         e.error().message.c_str());
            return 10;
        }
    }

    std::printf("[make_cdx] wrote %s with tags NAME, AGE, UPPER_NAME\n",
                out.string().c_str());

    // Side-stage: empty .fpt next to the .dbf so AdsOpenTable's
    // auto-attach finds a memo store when the DBF declares an M field.
    fs::path fpt = out;
    fpt.replace_extension(".fpt");
    fs::remove(fpt, ec);
    auto fpt_r = openads::drivers::fpt::FptMemo::create(fpt.string());
    if (!fpt_r) {
        std::fprintf(stderr, "[make_cdx] FptMemo::create failed: %s\n",
                     fpt_r.error().message.c_str());
        return 7;
    }
    std::printf("[make_cdx] wrote %s (empty memo store)\n",
                fpt.string().c_str());
    return 0;
}
