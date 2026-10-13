#ifndef OW_SHA256_H
#define OW_SHA256_H
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint8_t  buf[64];
    uint8_t  buflen;
    uint32_t bitlen_hi, bitlen_lo;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const uint8_t *data, uint16_t len);
void sha256_final(sha256_ctx *c, uint8_t out[32]);
void sha256(const uint8_t *data, uint16_t len, uint8_t out[32]);
/* SHA256(SHA256(data)) */
void sha256d(const uint8_t *data, uint16_t len, uint8_t out[32]);
/* BIP-340 tagged hash: SHA256(SHA256(tag)||SHA256(tag)||msg) */
void tagged_hash(const char *tag, const uint8_t *msg, uint16_t msglen, uint8_t out[32]);

#endif
