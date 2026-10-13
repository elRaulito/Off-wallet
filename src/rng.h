#ifndef OW_RNG_H
#define OW_RNG_H
#include <stdint.h>

/* One coin flip per button press, one bit of the key per flip. 128 bits is
   the same strength as a 12-word seed phrase, and on the 12-word path these
   are exactly the BIP-39 entropy bits. */
#define RNG_TARGET_BITS 128
#define RNG_BYTES       (RNG_TARGET_BITS / 8)

void    rng_init(void);
/* Throw away the flips so far and start the sequence again. */
void    rng_clear(void);
/* Keeps the counter racing while the game waits on other screens. */
void    rng_spin(void);
/* Blocks until the next press; records and returns that flip (1 = heads). */
uint8_t rng_flip(void);
uint8_t rng_bits(void);
/* Read flip i back, so the screen can show the whole sequence. */
uint8_t rng_bit(uint8_t i);
uint8_t rng_ready(void);
/* The flips themselves, first flip in the top bit of out[0]. */
void    rng_extract(uint8_t *out, uint8_t len);

#endif
