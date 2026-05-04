// Post-smoke probe: open data.cdx, list each tag's entries.
#include "drivers/cdx/cdx_index.h"

#include <cstdio>
#include <string>

using openads::drivers::cdx::CdxIndex;
using openads::drivers::IndexOpenMode;

int main(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : "data.cdx";
    auto tags_r = CdxIndex::list_tags(path);
    if (!tags_r) {
        std::fprintf(stderr, "list_tags fail: %s\n",
                     tags_r.error().message.c_str());
        return 1;
    }
    for (auto& tag : tags_r.value()) {
        std::printf("=== tag '%s' ===\n", tag.c_str());
        CdxIndex ix;
        if (auto r = ix.open_named(path, IndexOpenMode::Shared, tag); !r) {
            std::printf("  open_named fail: %s\n", r.error().message.c_str());
            continue;
        }
        auto seek = ix.seek_first();
        if (!seek) {
            std::printf("  seek_first fail: %s\n", seek.error().message.c_str());
            continue;
        }
        while (seek.value().positioned) {
            std::printf("  recno=%u key=[%s]\n",
                        seek.value().recno, ix.current_key().c_str());
            seek = ix.next();
            if (!seek) break;
        }
    }
    return 0;
}
