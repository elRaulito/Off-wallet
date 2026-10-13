#ifndef OW_WALLET_H
#define OW_WALLET_H
#include <stdint.h>
#include "secp256k1.h"

typedef struct {
    sc      k;      /* private scalar, little-endian */
    uint8_t c[32];  /* chain code */
} bip32_node;

typedef struct {
    uint8_t privkey[32];       /* big-endian, for WIF export */
    uint8_t internal_key[32];  /* x-only internal key, big-endian */
    uint8_t output_key[32];    /* x-only taproot output key, big-endian */
    char    address[64];       /* bc1p... (62 chars + NUL) */
} wallet_addr;

void    bip32_master(bip32_node *out, const uint8_t *seed, uint16_t seedlen);
uint8_t bip32_ckd(bip32_node *out, const bip32_node *parent, uint32_t index);
uint8_t taproot_output_key(uint8_t out_be[32], const ge_a *pubkey);
uint8_t wallet_single_p2tr(wallet_addr *out, const uint8_t privkey_be[32]);
uint8_t wallet_derive_p2tr(wallet_addr *out,
                           const uint8_t *seed, uint16_t seedlen,
                           uint32_t change, uint32_t index);

#endif
