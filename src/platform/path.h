#pragma once

#include <string>

namespace openads::platform {

// On Windows the filesystem is already case-insensitive; on POSIX this
// scans the parent directory once to find a case-folded match. Returns
// the input unchanged if no match exists.
std::string resolve_case_insensitive(const std::string& path);

} // namespace openads::platform
