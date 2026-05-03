#include "doctest.h"
#include "platform/time.h"

#include <thread>
#include <chrono>

using openads::platform::monotonic_nanos;
using openads::platform::utc_unix_micros;

TEST_CASE("monotonic_nanos is non-decreasing across two reads") {
    auto a = monotonic_nanos();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto b = monotonic_nanos();
    CHECK(b >= a);
}

TEST_CASE("utc_unix_micros falls within a sane range") {
    auto t = utc_unix_micros();
    // 2024-01-01 .. 2100-01-01 in microseconds.
    CHECK(t > 1'700'000'000'000'000ll);
    CHECK(t < 4'102'444'800'000'000ll);
}
