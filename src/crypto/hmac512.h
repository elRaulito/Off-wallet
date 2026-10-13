#ifndef OW_HMAC512_H
#define OW_HMAC512_H
#include <stdint.h>
#include "sha512.h"
#include "ow_bank.h"

/* Key schedule kept as two pre-fed SHA-512 states, so PBKDF2's 2048
   iterations don't re-hash the padded key every round. */
typedef struct {
    sha512_ctx inner;
    sha512_ctx outer;
} hmac512_key;

/* Called once per PBKDF2 iteration when non-NULL. */
extern void (*pbkdf2_progress_cb)(void);

void hmac512_setkey(hmac512_key *k, const uint8_t *key, uint16_t keylen);
void hmac512_init(sha512_ctx *c, const hmac512_key *k);
void hmac512_done(sha512_ctx *c, const hmac512_key *k, uint8_t out[64]);
void hmac512(const uint8_t *key, uint16_t keylen,
             const uint8_t *msg, uint16_t msglen, uint8_t out[64]) OW_BANKED;

/* PBKDF2-HMAC-SHA512 with dkLen = 64 (single output block). */
void pbkdf2_sha512_64(const uint8_t *pw, uint16_t pwlen,
                      const uint8_t *salt, uint16_t saltlen,
                      uint16_t iters, uint8_t out[64]) OW_BANKED;
#endif
