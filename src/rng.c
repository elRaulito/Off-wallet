/* Entropy: one coin flip per button press.

   A Game Boy has no hardware random number generator and no clock. Each
   press is one coin flip, and the flip is drawn on screen as it arrives, so
   the whole secret is visible while it is being made.

   The bit is the parity of the instant the press is detected. A counter
   races in a tight polling loop, pinning that instant to a few dozen clock
   cycles, and it is read together with the hardware timer (262144 Hz, one
   tick every 3.8 us), the divider and the LCD line. A press cannot be aimed
   to the microsecond, so the parity is a fair coin.

   128 flips give 128 bits, the same strength as a 12-word seed phrase. On
   the 12-word path those bits are the BIP-39 entropy itself, so the flips on
   the screen reproduce the wallet in any other BIP-39 tool; on the
   single-key path the private key is SHA-256 of the same bits. The
   heads/tails tally drawn alongside is there to be checked before the
   sequence is kept. */
#include <gb/gb.h>
#include <gb/hardware.h>
#include "rng.h"

static uint8_t  flips[RNG_BYTES];
static uint8_t  nflips;
static uint16_t spin;

void rng_init(void)
{
    /* TAC = 0x05: timer enabled at 262144 Hz, the fastest tick available. */
    TAC_REG = 0x05;
    TMA_REG = 0x00;

    /* Whatever the RAM held at power-on differs between units and between
       boots. It only shifts the counter's phase, but it costs nothing. */
    spin  = (uint16_t)(*(volatile uint8_t *)0xC000);
    spin |= (uint16_t)(*(volatile uint8_t *)0xC001) << 8;
    spin += (uint16_t)DIV_REG;

    rng_clear();
}

void rng_clear(void)
{
    uint8_t i;
    for (i = 0; i < RNG_BYTES; i++) flips[i] = 0;
    nflips = 0;
}

void rng_spin(void)
{
    spin++;
}

uint8_t rng_flip(void)
{
    uint8_t bit;

    /* Wait for everything to come up, then for the next button to go down,
       polling as fast as the CPU allows. One press, one flip. */
    while (joypad()) spin++;
    while (!joypad()) spin++;

    bit = (uint8_t)((spin ^ TIMA_REG ^ DIV_REG ^ LY_REG) & 1);
    if (nflips < RNG_TARGET_BITS) {
        if (bit) flips[nflips >> 3] |= (uint8_t)(0x80 >> (nflips & 7));
        nflips++;
    }
    return bit;
}

uint8_t rng_bits(void)
{
    return nflips;
}

uint8_t rng_bit(uint8_t i)
{
    return (uint8_t)((flips[i >> 3] >> (7 - (i & 7))) & 1);
}

uint8_t rng_ready(void)
{
    return (uint8_t)(nflips >= RNG_TARGET_BITS);
}

void rng_extract(uint8_t *out, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++) out[i] = (i < RNG_BYTES) ? flips[i] : 0;
}
