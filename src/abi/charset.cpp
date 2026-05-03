#include "abi/charset.h"

#include <algorithm>
#include <cstring>

namespace openads::abi {

std::string to_internal(const std::uint8_t* p, std::size_t n) {
    if (p == nullptr) return {};
    if (n == 0) {
        // ACE convention: NUL-terminated when length is 0.
        n = std::strlen(reinterpret_cast<const char*>(p));
    }
    return std::string(reinterpret_cast<const char*>(p), n);
}

void copy_to_caller(std::uint8_t* dst, std::uint16_t* dst_len_inout,
                    const std::string& src) noexcept {
    if (dst == nullptr || dst_len_inout == nullptr) return;
    std::uint16_t cap = *dst_len_inout;
    std::uint16_t n   = static_cast<std::uint16_t>(
        std::min<std::size_t>(src.size(), cap == 0 ? 0 : cap - 1));
    std::memcpy(dst, src.data(), n);
    if (cap > 0) dst[n] = '\0';
    *dst_len_inout = n;
}

} // namespace openads::abi
