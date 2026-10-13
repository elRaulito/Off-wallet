#include "base58.h"
#include "sha256.h"

static const char B58[59] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/* Base58 of a byte string, by repeated division of the big-endian number.
   Returns the encoded length. */
static uint8_t base58_encode(char *out, const uint8_t *data, uint8_t len)
{
    uint8_t buf[40];
    uint8_t i, zeros = 0, n = 0, start;
    char tmp[64];

    for (i = 0; i < len; i++) buf[i] = data[i];
    while (zeros < len && buf[zeros] == 0) zeros++;

    start = zeros;
    while (start < len) {
        uint16_t rem = 0;
        /* divide buf[start..len) by 58 in place, big-endian */
        for (i = start; i < len; i++) {
            uint16_t cur = (uint16_t)(rem << 8) | buf[i];
            buf[i] = (uint8_t)(cur / 58);
            rem = cur % 58;
        }
        tmp[n++] = B58[rem];
        while (start < len && buf[start] == 0) start++;
    }

    for (i = 0; i < zeros; i++) out[i] = '1';
    /* the digits came out least-significant first */
    for (i = 0; i < n; i++) out[zeros + i] = tmp[n - 1 - i];
    out[zeros + n] = 0;
    return (uint8_t)(zeros + n);
}

/* WIF for a compressed-pubkey key on mainnet:
   base58check(0x80 || privkey(32) || 0x01) */
uint8_t wif_encode(char *out, const uint8_t privkey_be[32])
{
    uint8_t payload[38];
    uint8_t chk[32];
    uint8_t i;

    payload[0] = 0x80;
    for (i = 0; i < 32; i++) payload[1 + i] = privkey_be[i];
    payload[33] = 0x01;
    sha256d(payload, 34, chk);
    for (i = 0; i < 4; i++) payload[34 + i] = chk[i];
    return base58_encode(out, payload, 38);
}
