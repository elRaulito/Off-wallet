#ifndef OW_BASE58_H
#define OW_BASE58_H
#include <stdint.h>

/* Mainnet WIF (compressed) for a big-endian private key. Needs 53 bytes. */
uint8_t wif_encode(char *out, const uint8_t privkey_be[32]);

#endif
