#include "engine/aes.h"

#include "openads_aes.h"

#include <cstring>

namespace openads::engine {

namespace {

bool valid_key_size(AesKeyBits bits, std::size_t n) {
    return (bits == AesKeyBits::Aes128 && n == 16) ||
           (bits == AesKeyBits::Aes256 && n == 32);
}

} // namespace

util::Result<Aes>
Aes::from_key(AesKeyBits bits, const std::vector<std::uint8_t>& key) {
    if (!valid_key_size(bits, key.size())) {
        return util::Error{5000, 0, "AES key size mismatches mode", ""};
    }
    Aes out;
    out.bits_ = bits;
    if (bits == AesKeyBits::Aes128) {
        if (openads_aes128_ctx_size() > out.ctx_.size()) {
            return util::Error{5000, 0, "AES-128 ctx larger than buffer", ""};
        }
        openads_aes128_init_ctx(out.ctx_.data(), key.data());
    } else {
        if (openads_aes256_ctx_size() > out.ctx_.size()) {
            return util::Error{5000, 0, "AES-256 ctx larger than buffer", ""};
        }
        openads_aes256_init_ctx(out.ctx_.data(), key.data());
    }
    return out;
}

void Aes::encrypt_block(std::uint8_t block[16]) const {
    if (bits_ == AesKeyBits::Aes128) {
        openads_aes128_ecb_encrypt(ctx_.data(), block);
    } else {
        openads_aes256_ecb_encrypt(ctx_.data(), block);
    }
}

void Aes::decrypt_block(std::uint8_t block[16]) const {
    if (bits_ == AesKeyBits::Aes128) {
        openads_aes128_ecb_decrypt(ctx_.data(), block);
    } else {
        openads_aes256_ecb_decrypt(ctx_.data(), block);
    }
}

util::Result<void>
Aes::encrypt_ecb(std::uint8_t* buf, std::size_t n) const {
    if (n % 16 != 0) {
        return util::Error{5000, 0, "AES ECB length not multiple of 16", ""};
    }
    for (std::size_t off = 0; off < n; off += 16) {
        encrypt_block(buf + off);
    }
    return {};
}

util::Result<void>
Aes::decrypt_ecb(std::uint8_t* buf, std::size_t n) const {
    if (n % 16 != 0) {
        return util::Error{5000, 0, "AES ECB length not multiple of 16", ""};
    }
    for (std::size_t off = 0; off < n; off += 16) {
        decrypt_block(buf + off);
    }
    return {};
}

} // namespace openads::engine
