#include "doctest.h"
#include "util/log.h"

#include <sstream>

using openads::util::Log;
using openads::util::LogLevel;

TEST_CASE("Log respects the configured level threshold") {
    std::ostringstream out;
    Log log{LogLevel::Info, &out};

    log.write(LogLevel::Debug, "debug-line");
    log.write(LogLevel::Info,  "info-line");
    log.write(LogLevel::Error, "err-line");

    const std::string buf = out.str();
    CHECK(buf.find("debug-line") == std::string::npos);
    CHECK(buf.find("info-line")  != std::string::npos);
    CHECK(buf.find("err-line")   != std::string::npos);
}

TEST_CASE("Log emits the level prefix") {
    std::ostringstream out;
    Log log{LogLevel::Trace, &out};
    log.write(LogLevel::Trace, "tag");
    CHECK(out.str().find("TRACE") != std::string::npos);
    CHECK(out.str().find("tag")   != std::string::npos);
}

TEST_CASE("Log discards output when sink is null") {
    Log log{LogLevel::Trace, nullptr};
    log.write(LogLevel::Error, "ignored");
    // No crash, no UB. Nothing else to assert.
    CHECK(true);
}

TEST_CASE("Log parses level from environment-style string") {
    CHECK(openads::util::log_level_from_string("trace") == LogLevel::Trace);
    CHECK(openads::util::log_level_from_string("DEBUG") == LogLevel::Debug);
    CHECK(openads::util::log_level_from_string("info")  == LogLevel::Info);
    CHECK(openads::util::log_level_from_string("warn")  == LogLevel::Warn);
    CHECK(openads::util::log_level_from_string("error") == LogLevel::Error);
    CHECK(openads::util::log_level_from_string("nonsense") == LogLevel::Info);
}
