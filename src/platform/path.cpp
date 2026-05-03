#include "platform/path.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace openads::platform {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return s;
}

} // namespace

std::string resolve_case_insensitive(const std::string& path) {
    fs::path p(path);
    fs::path parent = p.parent_path();
    if (parent.empty()) parent = ".";

    std::error_code ec;
    if (!fs::is_directory(parent, ec)) {
        // Parent directory does not exist: nothing to resolve.
        return path;
    }

    const std::string leaf      = p.filename().string();
    const std::string leaf_low  = to_lower(leaf);

    // First pass: prefer an exact-case match if one exists. This keeps
    // the input verbatim when the case is already correct.
    for (const auto& entry : fs::directory_iterator(parent, ec)) {
        if (ec) break;
        if (entry.path().filename().string() == leaf) {
            return entry.path().string();
        }
    }

    // Second pass: case-insensitive match returns the on-disk casing.
    for (const auto& entry : fs::directory_iterator(parent, ec)) {
        if (ec) break;
        if (to_lower(entry.path().filename().string()) == leaf_low) {
            return entry.path().string();
        }
    }

    return path;
}

} // namespace openads::platform
