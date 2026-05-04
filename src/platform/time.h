#pragma once

#include <cstdint>
#include <string>

namespace openads::platform {

std::int64_t monotonic_nanos();   // monotonic, never decreases
std::int64_t utc_unix_micros();   // microseconds since 1970-01-01 UTC

// Local host name (from gethostname / GetComputerNameA). Empty string
// if the system call fails. Used by AdsGetServerName so a local-mode
// connection reports something more useful than "".
std::string host_name();

// Decompose the current local wall clock into ISO date / time strings
// and milliseconds since midnight, all materialised together so a
// single AdsGetServerTime call observes a consistent moment.
struct LocalWallClock {
    std::string date;          // "YYYY-MM-DD"
    std::string time;          // "HH:MM:SS"
    std::int32_t ms_of_day;    // 0..86_399_999
};
LocalWallClock now_local();

} // namespace openads::platform
