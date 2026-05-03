#pragma once

#include <cstdint>

namespace openads::platform {

std::int64_t monotonic_nanos();   // monotonic, never decreases
std::int64_t utc_unix_micros();   // microseconds since 1970-01-01 UTC

} // namespace openads::platform
