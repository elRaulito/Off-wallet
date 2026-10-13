#ifndef OW_SHA512_H
#define OW_SHA512_H
#include <stdint.h>
#include "ow_bank.h"

/* State words are eight little-endian bytes each; see sha512.c for why that
   beats a pair of 32-bit halves on this CPU. */
typedef struct {
    uint8_t  h[8][8];
    uint8_t  buf[128];
    uint8_t  buflen;
    uint32_t bitlen;      /* message length in bits, low 32 */
    uint32_t bitlen_hi;
} sha512_ctx;

#ifdef OW_GAMEBOY
/* 1 = use the SM83 assembly compression, 0 = the portable C one. */
extern uint8_t sha512_use_asm;
void sha512_compress_dbg(uint8_t *hstate, const uint8_t *block) OW_BANKED;
#endif

void sha512_init(sha512_ctx *c);
void sha512_update(sha512_ctx *c, const uint8_t *data, uint16_t len);
void sha512_final(sha512_ctx *c, uint8_t out[64]);
/* One-shot hash. BANKED: the only sha512 entry point callable from other
   banks; init/update/final are for callers already in bank 4 (hmac512). */
void sha512(const uint8_t *data, uint16_t len, uint8_t out[64]) OW_BANKED;

#endif
