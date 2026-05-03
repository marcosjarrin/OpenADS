/* Stable AES C ABI exposed to engine/aes.cpp.
 * Both AES-128 and AES-256 are linked simultaneously by compiling
 * aes.c twice with different AES_KEYLEN macros. */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void   openads_aes128_init_ctx   (unsigned char* ctx_buf, const unsigned char* key);
void   openads_aes128_ecb_encrypt(const unsigned char* ctx_buf, unsigned char* buf);
void   openads_aes128_ecb_decrypt(const unsigned char* ctx_buf, unsigned char* buf);
size_t openads_aes128_ctx_size   (void);

void   openads_aes256_init_ctx   (unsigned char* ctx_buf, const unsigned char* key);
void   openads_aes256_ecb_encrypt(const unsigned char* ctx_buf, unsigned char* buf);
void   openads_aes256_ecb_decrypt(const unsigned char* ctx_buf, unsigned char* buf);
size_t openads_aes256_ctx_size   (void);

#ifdef __cplusplus
}
#endif
