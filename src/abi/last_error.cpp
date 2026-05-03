#include "abi/last_error.h"

namespace openads::abi {

namespace {
thread_local std::int32_t  g_code = 0;
thread_local std::string   g_msg;
} // namespace

void set_last_error(const util::Error& e) noexcept {
    g_code = e.code;
    g_msg  = e.message;
}

void clear_last_error() noexcept {
    g_code = 0;
    g_msg.clear();
}

std::int32_t last_error_code() noexcept { return g_code; }
const std::string& last_error_message() noexcept { return g_msg; }

} // namespace openads::abi
