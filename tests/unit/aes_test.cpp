#include "doctest.h"
#include "engine/aes.h"

#include <cstdint>
#include <cstring>
#include <vector>

using openads::engine::Aes;
using openads::engine::AesKeyBits;

TEST_CASE("AES-128 encrypts and decrypts a single block (FIPS-197 test vector)") {
    // FIPS-197 Appendix B: key = 2b7e1516 28aed2a6 abf71588 09cf4f3c
    std::vector<std::uint8_t> key = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
    };
    auto aes = Aes::from_key(AesKeyBits::Aes128, key);
    REQUIRE(aes.has_value());

    std::uint8_t block[16] = {
        0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
        0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34
    };
    aes.value().encrypt_block(block);
    const std::uint8_t expected_ct[16] = {
        0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,
        0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32
    };
    CHECK(std::memcmp(block, expected_ct, 16) == 0);

    aes.value().decrypt_block(block);
    const std::uint8_t expected_pt[16] = {
        0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
        0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34
    };
    CHECK(std::memcmp(block, expected_pt, 16) == 0);
}

TEST_CASE("AES-256 encrypts and decrypts a single block (NIST test vector)") {
    // NIST SP 800-38A Appendix F.1.5 / F.1.6 ECB-AES256 test vector
    // Key: 603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4
    std::vector<std::uint8_t> key = {
        0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
        0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
        0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
        0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
    };
    auto aes = Aes::from_key(AesKeyBits::Aes256, key);
    REQUIRE(aes.has_value());

    // Plaintext block 1: 6bc1bee22e409f96e93d7e117393172a
    std::uint8_t block[16] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    aes.value().encrypt_block(block);
    // Expected ciphertext: f3eed1bdb5d2a03c064b5a7e3db181f8
    const std::uint8_t expected_ct[16] = {
        0xf3,0xee,0xd1,0xbd,0xb5,0xd2,0xa0,0x3c,
        0x06,0x4b,0x5a,0x7e,0x3d,0xb1,0x81,0xf8
    };
    CHECK(std::memcmp(block, expected_ct, 16) == 0);

    aes.value().decrypt_block(block);
    const std::uint8_t expected_pt[16] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    CHECK(std::memcmp(block, expected_pt, 16) == 0);
}

TEST_CASE("AES round-trip across multiple blocks") {
    std::vector<std::uint8_t> key(16, 0x42);
    auto aes = Aes::from_key(AesKeyBits::Aes128, key);
    REQUIRE(aes.has_value());

    std::vector<std::uint8_t> data(64);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<std::uint8_t>(i);
    auto pt = data;
    REQUIRE(aes.value().encrypt_ecb(data.data(), data.size()).has_value());
    CHECK(data != pt);
    REQUIRE(aes.value().decrypt_ecb(data.data(), data.size()).has_value());
    CHECK(data == pt);
}

TEST_CASE("AES rejects mismatched key size") {
    std::vector<std::uint8_t> key(16, 0x00);
    auto a = Aes::from_key(AesKeyBits::Aes256, key);
    CHECK_FALSE(a.has_value());
}
