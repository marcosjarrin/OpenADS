/* Compile aes.c configured for AES-256, with renamed public symbols. */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define AES256 1
#define CBC 0
#define CTR 0
#define ECB 1

#define AES_init_ctx     openads_aes256_init_ctx_inner
#define AES_ECB_encrypt  openads_aes256_ecb_encrypt_inner
#define AES_ECB_decrypt  openads_aes256_ecb_decrypt_inner
#define AES_ctx          openads_aes256_ctx_t

#include "aes.c"

typedef struct openads_aes256_ctx_t openads_aes256_ctx_typedef;

void openads_aes256_init_ctx(unsigned char* ctx_buf, const unsigned char* key) {
    openads_aes256_init_ctx_inner((openads_aes256_ctx_typedef*)ctx_buf, key);
}
void openads_aes256_ecb_encrypt(const unsigned char* ctx_buf, unsigned char* buf) {
    openads_aes256_ecb_encrypt_inner((const openads_aes256_ctx_typedef*)ctx_buf, buf);
}
void openads_aes256_ecb_decrypt(const unsigned char* ctx_buf, unsigned char* buf) {
    openads_aes256_ecb_decrypt_inner((const openads_aes256_ctx_typedef*)ctx_buf, buf);
}
size_t openads_aes256_ctx_size(void) { return sizeof(openads_aes256_ctx_typedef); }
