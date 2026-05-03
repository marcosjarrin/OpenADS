#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace openads::abi {

// Phase 1 placeholder: M1 treats input/output as already-correct byte
// sequences. Real OEM/ANSI/UTF translation lands in M4 alongside the
// `*W` entry-point variants.
std::string to_internal(const std::uint8_t* p, std::size_t n);
void copy_to_caller(std::uint8_t* dst, std::uint16_t* dst_len_inout,
                    const std::string& src) noexcept;

} // namespace openads::abi
