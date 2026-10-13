#include "bech32.h"

static const char CHARSET[33] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static uint32_t polymod_step(uint32_t pre, uint8_t value)
{
    uint8_t b = (uint8_t)(pre >> 25);
    uint32_t v = ((pre & 0x1ffffffUL) << 5) ^ value;
    if (b & 1)  v ^= 0x3b6a57b2UL;
    if (b & 2)  v ^= 0x26508e6dUL;
    if (b & 4)  v ^= 0x1ea119faUL;
    if (b & 8)  v ^= 0x3d4233ddUL;
    if (b & 16) v ^= 0x2a1462b3UL;
    return v;
}

/* Encode a segwit address. hrp is "bc" or "tb"; witver 1 and a 32-byte
   program give a BIP-341 P2TR address (bech32m). Returns string length. */
uint8_t segwit_addr_encode(char *out, const char *hrp,
                           uint8_t witver, const uint8_t *prog, uint8_t prog_len)
{
    uint8_t data[80];
    uint8_t dlen = 0;
    uint32_t chk = 1;
    uint32_t acc = 0;
    uint8_t bits = 0;
    uint8_t i, n = 0;

    /* 8-bit -> 5-bit regrouping, witness version first */
    data[dlen++] = witver;
    for (i = 0; i < prog_len; i++) {
        acc = (acc << 8) | prog[i];
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            data[dlen++] = (uint8_t)((acc >> bits) & 31);
        }
    }
    if (bits) data[dlen++] = (uint8_t)((acc << (5 - bits)) & 31);

    /* checksum over expand(hrp) || data */
    for (i = 0; hrp[i]; i++) chk = polymod_step(chk, (uint8_t)(hrp[i] >> 5));
    chk = polymod_step(chk, 0);
    for (i = 0; hrp[i]; i++) chk = polymod_step(chk, (uint8_t)(hrp[i] & 31));
    for (i = 0; i < dlen; i++) chk = polymod_step(chk, data[i]);
    for (i = 0; i < 6; i++) chk = polymod_step(chk, 0);
    /* BIP-350: bech32m constant; witness v0 would use 1 (bech32) */
    chk ^= (witver == 0) ? 1UL : 0x2bc830a3UL;

    for (i = 0; hrp[i]; i++) out[n++] = hrp[i];
    out[n++] = '1';
    for (i = 0; i < dlen; i++) out[n++] = CHARSET[data[i]];
    for (i = 0; i < 6; i++) out[n++] = CHARSET[(chk >> (5 * (5 - i))) & 31];
    out[n] = 0;
    return n;
}
