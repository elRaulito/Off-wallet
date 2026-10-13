#ifndef OW_BECH32_H
#define OW_BECH32_H
#include <stdint.h>

uint8_t segwit_addr_encode(char *out, const char *hrp,
                           uint8_t witver, const uint8_t *prog, uint8_t prog_len);

#endif
