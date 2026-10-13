/* Benchmark ROM: times each crypto primitive on real hardware timing and
   leaves the frame counts in WRAM for tools/run_bench.py. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include <string.h>

#include "crypto/sha256.h"
#include "crypto/sha512.h"
#include "crypto/hmac512.h"
#include "crypto/secp256k1.h"
#include "crypto/wallet.h"

/* [0] = done, [1] = stage, [2..] = pairs of little-endian frame counts */
uint8_t bn_out[32];

static volatile uint16_t frames;
static void tick(void) { frames++; }

static void mark(uint8_t slot, uint16_t v)
{
    bn_out[2 + slot * 2] = (uint8_t)v;
    bn_out[3 + slot * 2] = (uint8_t)(v >> 8);
}

/* the official BIP-39 seed for "abandon ... about", so we can time the EC part
   without waiting for PBKDF2 */
static const uint8_t SEED[64] = {
  0x5e,0xb0,0x0b,0xbd,0xdc,0xf0,0x69,0x08,0x48,0x89,0xa8,0xab,0x91,0x55,0x56,0x81,
  0x65,0xf5,0xc4,0x53,0xcc,0xb8,0x5e,0x70,0x81,0x1a,0xae,0xd6,0xf6,0xda,0x5f,0xc1,
  0x9a,0x5a,0xc4,0x0b,0x38,0x9c,0xd3,0x70,0xd0,0x86,0x20,0x6d,0xec,0x8a,0xa6,0xc4,
  0x3d,0xae,0xa6,0x69,0x0f,0x20,0xad,0x3d,0x8d,0x48,0xb2,0xd2,0xce,0x9e,0x38,0xe4
};

void main(void)
{
    uint8_t i;
    fe a, b, r;
    sc k;
    ge_a P;
    uint8_t h[32];
    uint8_t out64[64];
    wallet_addr w;

    if (_cpu == CGB_TYPE) cpu_fast();
    for (i = 0; i < sizeof(bn_out); i++) bn_out[i] = 0;

    add_VBL(tick);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();
    DISPLAY_ON;
    printf("OffWallet bench\n\n");

    for (i = 0; i < 32; i++) { a[i] = (uint8_t)(i * 7 + 1); b[i] = (uint8_t)(i * 13 + 3); }

    /* 64 field multiplications */
    frames = 0;
    for (i = 0; i < 64; i++) fe_mul(r, a, b);
    mark(0, frames);
    printf("64 fe_mul: %u f\n", frames);
    bn_out[1] = 1;

    /* one modular inversion */
    frames = 0;
    fe_inv(r, a);
    mark(1, frames);
    printf("fe_inv: %u f\n", frames);
    bn_out[1] = 2;

    /* one fixed-base scalar multiplication */
    for (i = 0; i < 32; i++) k[i] = (uint8_t)(i * 11 + 5);
    frames = 0;
    secp_mul_g(&P, k);
    mark(2, frames);
    printf("k*G: %u f\n", frames);
    bn_out[1] = 3;

    /* 16 SHA-256 compressions (one 1000-byte hash is ~16 blocks) */
    frames = 0;
    for (i = 0; i < 16; i++) sha256((const uint8_t *)SEED, 64, h);
    mark(3, frames);
    printf("16 sha256: %u f\n", frames);
    bn_out[1] = 4;

    /* 16 SHA-512 compressions */
    frames = 0;
    for (i = 0; i < 16; i++) sha512((const uint8_t *)SEED, 64, out64);
    mark(4, frames);
    printf("16 sha512: %u f\n", frames);
    bn_out[1] = 5;

    /* full BIP-32 + taproot derivation from a known seed (no PBKDF2) */
    frames = 0;
    if (wallet_derive_p2tr(&w, SEED, 64, 0, 0)) {
        mark(5, frames);
        printf("derive: %u f\n", frames);
        printf("%c%c%c%c%c\n", w.address[0], w.address[1], w.address[2],
               w.address[3], w.address[4]);
        /* stash the first 8 bytes of the output key as a correctness spot-check */
        for (i = 0; i < 8; i++) bn_out[16 + i] = w.output_key[i];
    }
    bn_out[1] = 6;
    bn_out[0] = 1;
    while (1) vsync();
}
