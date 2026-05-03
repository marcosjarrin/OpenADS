#include "platform/time.h"

#include <chrono>

namespace openads::platform {

std::int64_t monotonic_nanos() {
    using clock = std::chrono::steady_clock;
    auto d = clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
}

std::int64_t utc_unix_micros() {
    using clock = std::chrono::system_clock;
    auto d = clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

} // namespace openads::platform
