#ifdef OW_GAMEBOY
#pragma bank 4
#endif
#include "hmac512.h"

/* Progress hook, called once per PBKDF2 iteration when non-NULL. */
void (*pbkdf2_progress_cb)(void) = 0;

void hmac512_setkey(hmac512_key *k, const uint8_t *key, uint16_t keylen)
{
    uint8_t pad[128];
    uint8_t kh[64];
    uint8_t i;

    if (keylen > 128) {
        sha512(key, keylen, kh);
        key = kh; keylen = 64;
    }
    for (i = 0; i < 128; i++) pad[i] = 0x36;
    for (i = 0; i < (uint8_t)keylen; i++) pad[i] ^= key[i];
    sha512_init(&k->inner);
    sha512_update(&k->inner, pad, 128);

    for (i = 0; i < 128; i++) pad[i] = 0x5c;
    for (i = 0; i < (uint8_t)keylen; i++) pad[i] ^= key[i];
    sha512_init(&k->outer);
    sha512_update(&k->outer, pad, 128);
}

void hmac512_init(sha512_ctx *c, const hmac512_key *k)
{
    *c = k->inner;
}

void hmac512_done(sha512_ctx *c, const hmac512_key *k, uint8_t out[64])
{
    uint8_t ih[64];
    sha512_ctx o;
    sha512_final(c, ih);
    o = k->outer;
    sha512_update(&o, ih, 64);
    sha512_final(&o, out);
}

void hmac512(const uint8_t *key, uint16_t keylen,
             const uint8_t *msg, uint16_t msglen, uint8_t out[64]) OW_BANKED
{
    hmac512_key k;
    sha512_ctx c;
    hmac512_setkey(&k, key, keylen);
    hmac512_init(&c, &k);
    sha512_update(&c, msg, msglen);
    hmac512_done(&c, &k, out);
}

void pbkdf2_sha512_64(const uint8_t *pw, uint16_t pwlen,
                      const uint8_t *salt, uint16_t saltlen,
                      uint16_t iters, uint8_t out[64]) OW_BANKED
{
    hmac512_key k;
    sha512_ctx c;
    uint8_t u[64];
    uint8_t be1[4];
    uint8_t i;
    uint16_t it;

    hmac512_setkey(&k, pw, pwlen);

    /* U1 = PRF(pw, salt || INT_BE(1)) */
    be1[0] = 0; be1[1] = 0; be1[2] = 0; be1[3] = 1;
    hmac512_init(&c, &k);
    sha512_update(&c, salt, saltlen);
    sha512_update(&c, be1, 4);
    hmac512_done(&c, &k, u);
    for (i = 0; i < 64; i++) out[i] = u[i];

    for (it = 1; it < iters; it++) {
        if (pbkdf2_progress_cb) pbkdf2_progress_cb();
        hmac512_init(&c, &k);
        sha512_update(&c, u, 64);
        hmac512_done(&c, &k, u);
        for (i = 0; i < 64; i++) out[i] ^= u[i];
    }
}
