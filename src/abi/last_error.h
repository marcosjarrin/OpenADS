#pragma once

#include "util/result.h"

#include <cstdint>
#include <string>

namespace openads::abi {

void set_last_error(const util::Error& e) noexcept;
void clear_last_error() noexcept;

std::int32_t  last_error_code() noexcept;
const std::string& last_error_message() noexcept;

} // namespace openads::abi
