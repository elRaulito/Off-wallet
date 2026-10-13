#include <stdio.h>
#include <string.h>
#include "../src/crypto/bip39.h"
#include "../src/crypto/wallet.h"

int main(void)
{
    uint16_t idx[12];
    char mn[128];
    uint8_t ent[16];
    uint8_t seed[64];
    wallet_addr w;
    int i;

    /* all-zero entropy must give the canonical "abandon ... about" mnemonic */
    for (i = 0; i < 16; i++) ent[i] = 0;
    bip39_from_entropy(idx, ent);
    bip39_mnemonic(mn, idx);
    printf("zero-entropy mnemonic: %s\n", mn);
    printf("indices:");
    for (i = 0; i < 12; i++) printf(" %u", idx[i]);
    printf("\n");

    bip39_seed(seed, mn, (uint8_t)strlen(mn));
    printf("seed: ");
    for (i = 0; i < 64; i++) printf("%02x", seed[i]);
    printf("\n");
    if (wallet_derive_p2tr(&w, seed, 64, 0, 0))
        printf("address: %s\n", w.address);

    /* a second, non-trivial entropy value */
    for (i = 0; i < 16; i++) ent[i] = (uint8_t)(0x0f + i * 0x11);
    bip39_from_entropy(idx, ent);
    bip39_mnemonic(mn, idx);
    printf("entropy2: ");
    for (i = 0; i < 16; i++) printf("%02x", ent[i]);
    printf("\nmnemonic2: %s\n", mn);
    bip39_seed(seed, mn, (uint8_t)strlen(mn));
    if (wallet_derive_p2tr(&w, seed, 64, 0, 0))
        printf("address2: %s\n", w.address);
    return 0;
}
