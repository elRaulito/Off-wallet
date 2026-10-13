/* One compression step, in isolation, with its intermediate state exposed so
   tools/debug_sha.py can compare each stage against Python. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "crypto/sha512.h"

extern uint8_t sha_v[128];
extern uint8_t sha_w[256];
extern uint8_t sha_t1[8], sha_s0[8], sha_s1[8], sha_x[8];
extern uint8_t sha_rounds;

uint8_t d_out[500];
static uint8_t state[64];
static uint8_t block[128];

/* SHA-512 initial hash values, little-endian per word */
static const uint8_t H0[64] = {
  0x08,0xc9,0xbc,0xf3,0x67,0xe6,0x09,0x6a, 0x3b,0xa7,0xca,0x84,0x85,0xae,0x67,0xbb,
  0x2b,0xf8,0x94,0xfe,0x72,0xf3,0x6e,0x3c, 0xf1,0x36,0x1d,0x5f,0x3a,0xf5,0x4f,0xa5,
  0xd1,0x82,0xe6,0xad,0x7f,0x52,0x0e,0x51, 0x1f,0x6c,0x3e,0x2b,0x8c,0x68,0x05,0x9b,
  0x6b,0xbd,0x41,0xfb,0xab,0xd9,0x83,0x1f, 0x79,0x21,0x7e,0x13,0x19,0xcd,0xe0,0x5b
};

void main(void)
{
    uint8_t i;
    if (_cpu == CGB_TYPE) cpu_fast();
    DISPLAY_ON;

    for (i = 0; i < 64; i++) state[i] = H0[i];
    for (i = 0; i < 128; i++) block[i] = (uint8_t)(i + 1);   /* 1,2,3,... */

    /* Only this debug ROM ever changes this: the game and every other test
       leave it at the 80 rounds sha512.c initialises it to. */
    sha_rounds = 1;
    sha512_compress_dbg(state, block);

    {   /* state, then the whole W ring, then the working slots */
        uint16_t k;
        for (k = 0; k < 64; k++) d_out[8 + k] = state[k];
        for (k = 0; k < 256; k++) d_out[72 + k] = sha_w[k];
        for (k = 0; k < 128; k++) d_out[328 + k] = sha_v[k];
        for (k = 0; k < 8; k++) {
            d_out[456 + k] = sha_t1[k];
            d_out[464 + k] = sha_s0[k];
            d_out[472 + k] = sha_s1[k];
            d_out[480 + k] = sha_x[k];
        }
    }
    d_out[0] = 1;
    while (1) vsync();
}
