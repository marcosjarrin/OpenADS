#pragma once

#include <ostream>
#include <string_view>

namespace openads::util {

enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4 };

class Log {
public:
    Log(LogLevel threshold, std::ostream* sink) noexcept
        : threshold_(threshold), sink_(sink) {}

    void write(LogLevel level, std::string_view message) noexcept;

    LogLevel threshold() const noexcept { return threshold_; }

private:
    LogLevel       threshold_;
    std::ostream*  sink_;
};

LogLevel log_level_from_string(std::string_view s) noexcept;

} // namespace openads::util
