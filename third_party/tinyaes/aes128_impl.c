/* Compile aes.c configured for AES-128, with public symbols renamed
 * to a stable openads_aes128_* prefix so both 128 and 256 variants
 * coexist in the same binary. */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define AES128 1
#define CBC 0
#define CTR 0
#define ECB 1

#define AES_init_ctx     openads_aes128_init_ctx_inner
#define AES_ECB_encrypt  openads_aes128_ecb_encrypt_inner
#define AES_ECB_decrypt  openads_aes128_ecb_decrypt_inner
#define AES_ctx          openads_aes128_ctx_t

#include "aes.c"

typedef struct openads_aes128_ctx_t openads_aes128_ctx_typedef;

void openads_aes128_init_ctx(unsigned char* ctx_buf, const unsigned char* key) {
    openads_aes128_init_ctx_inner((openads_aes128_ctx_typedef*)ctx_buf, key);
}
void openads_aes128_ecb_encrypt(const unsigned char* ctx_buf, unsigned char* buf) {
    openads_aes128_ecb_encrypt_inner((const openads_aes128_ctx_typedef*)ctx_buf, buf);
}
void openads_aes128_ecb_decrypt(const unsigned char* ctx_buf, unsigned char* buf) {
    openads_aes128_ecb_decrypt_inner((const openads_aes128_ctx_typedef*)ctx_buf, buf);
}
size_t openads_aes128_ctx_size(void) { return sizeof(openads_aes128_ctx_typedef); }
