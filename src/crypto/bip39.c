#include "bip39.h"
#include "bip39_words.h"
#include "sha256.h"
#include "hmac512.h"
#include "ow_bank.h"

/* 128 bits of entropy -> 12 word indices.
   bits = ENT(128) || checksum(4), split into 12 groups of 11 bits. */
void bip39_from_entropy(uint16_t idx[12], const uint8_t ent[16])
{
    uint8_t h[32];
    uint8_t bits[18];                   /* 17 used; [17] pads the last read */
    uint8_t i;

    sha256(ent, 16, h);
    for (i = 0; i < 16; i++) bits[i] = ent[i];
    bits[16] = h[0];                    /* only the top 4 bits are the checksum */
    bits[17] = 0;

    for (i = 0; i < 12; i++) {
        uint16_t bitpos = (uint16_t)i * 11;
        uint8_t  byi    = (uint8_t)(bitpos >> 3);
        uint8_t  off    = (uint8_t)(bitpos & 7);
        /* 24-bit window, then align so the wanted bit is at the top */
        uint32_t w = ((uint32_t)bits[byi] << 16) |
                     ((uint32_t)bits[byi + 1] << 8) |
                      (uint32_t)bits[byi + 2];
        w <<= off;
        w &= 0xffffffUL;
        idx[i] = (uint16_t)((w >> 13) & 0x7ff);
    }
}

/* Copy word `index` (0..2047) into dst, NUL-terminated. Returns its length. */
uint8_t bip39_word(char *dst, uint16_t index)
{
    const uint8_t *p;
    uint8_t i;
    OW_BANK_SAVE();
    p = bip39_words_ptr((uint16_t)(index * BIP39_STRIDE));
    for (i = 0; i < BIP39_STRIDE && p[i]; i++) dst[i] = (char)p[i];
    dst[i] = 0;
    OW_BANK_RESTORE();
    return i;
}

/* Build the space-separated mnemonic string used as the PBKDF2 password. */
uint8_t bip39_mnemonic(char *dst, const uint16_t idx[12])
{
    uint8_t n = 0;
    uint8_t i, j, l;
    char w[BIP39_STRIDE + 1];
    for (i = 0; i < 12; i++) {
        if (i) dst[n++] = ' ';
        l = bip39_word(w, idx[i]);
        for (j = 0; j < l; j++) dst[n++] = w[j];
    }
    dst[n] = 0;
    return n;
}

/* mnemonic string -> 64-byte BIP-39 seed (empty passphrase). */
void bip39_seed(uint8_t seed[64], const char *mnemonic, uint8_t len)
{
    pbkdf2_sha512_64((const uint8_t *)mnemonic, len,
                     (const uint8_t *)"mnemonic", 8, 2048, seed);
}
