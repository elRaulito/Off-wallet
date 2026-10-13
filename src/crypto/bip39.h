#ifndef OW_BIP39_H
#define OW_BIP39_H
#include <stdint.h>

/* 12-word English BIP-39 mnemonics from 128 bits of entropy. */
void    bip39_from_entropy(uint16_t idx[12], const uint8_t ent[16]);
uint8_t bip39_word(char *dst, uint16_t index);
uint8_t bip39_mnemonic(char *dst, const uint16_t idx[12]);
void    bip39_seed(uint8_t seed[64], const char *mnemonic, uint8_t len);

#endif
