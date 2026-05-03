#include "util/log.h"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <string>

namespace openads::util {

namespace {

const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

std::string lower(std::string_view s) {
    std::string out{s};
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return out;
}

} // namespace

void Log::write(LogLevel level, std::string_view message) noexcept {
    if (sink_ == nullptr) return;
    if (static_cast<int>(level) < static_cast<int>(threshold_)) return;
    (*sink_) << level_name(level) << ' ' << message << '\n';
}

LogLevel log_level_from_string(std::string_view s) noexcept {
    const std::string norm = lower(s);
    if (norm == "trace") return LogLevel::Trace;
    if (norm == "debug") return LogLevel::Debug;
    if (norm == "info")  return LogLevel::Info;
    if (norm == "warn")  return LogLevel::Warn;
    if (norm == "error") return LogLevel::Error;
    return LogLevel::Info;
}

} // namespace openads::util
