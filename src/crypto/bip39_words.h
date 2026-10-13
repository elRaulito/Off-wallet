/* AUTO-GENERATED: BIP-39 English wordlist accessor. */
#ifndef OW_BIP39_WORDS_H
#define OW_BIP39_WORDS_H
#include <stdint.h>

#define BIP39_STRIDE     8
#define BIP39_NWORDS     2048
#define BIP39_WORDS_BANK 3

extern const uint8_t BIP39_WORDS[16384];

#ifdef OW_GAMEBOY
#include <gb/gb.h>
static const uint8_t *bip39_words_ptr(uint16_t off)
{
    SWITCH_ROM(BIP39_WORDS_BANK);
    return &BIP39_WORDS[off];
}
#else
static const uint8_t *bip39_words_ptr(uint16_t off)
{
    return &BIP39_WORDS[off];
}
#endif

#endif
