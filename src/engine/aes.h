#pragma once

#include "util/result.h"

#include <array>
#include <cstdint>
#include <vector>

namespace openads::engine {

enum class AesKeyBits { Aes128 = 128, Aes256 = 256 };

class Aes {
public:
    Aes() = default;
    Aes(const Aes&) = delete;
    Aes& operator=(const Aes&) = delete;
    Aes(Aes&&) noexcept = default;
    Aes& operator=(Aes&&) noexcept = default;
    ~Aes() = default;

    // Configures the key. ECB mode (record-level, matching ADS legacy).
    static util::Result<Aes>
        from_key(AesKeyBits bits, const std::vector<std::uint8_t>& key);

    // In-place encrypt / decrypt of a single 16-byte block.
    void encrypt_block(std::uint8_t block[16]) const;
    void decrypt_block(std::uint8_t block[16]) const;

    // Whole-buffer ECB. Buffer length must be a multiple of 16.
    util::Result<void> encrypt_ecb(std::uint8_t* buf, std::size_t n) const;
    util::Result<void> decrypt_ecb(std::uint8_t* buf, std::size_t n) const;

    AesKeyBits bits() const noexcept { return bits_; }

private:
    AesKeyBits                  bits_ = AesKeyBits::Aes128;
    // Round-key context lives in tinyaes' AES_ctx struct; we store the
    // bytes opaquely sized to the maximum (240 bytes for AES-256 round keys).
    std::array<std::uint8_t, 256> ctx_{};
};

} // namespace openads::engine
