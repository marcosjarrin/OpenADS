#pragma once

#include <cstdint>

namespace openads::platform {

// Current resident set size (physical memory) of this process, in
// bytes. Returns 0 if it cannot be determined.
std::uint64_t process_rss_bytes();

}  // namespace openads::platform
