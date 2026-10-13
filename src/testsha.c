/* Compares the SM83 assembly SHA-512 against the C one on real hardware, and
   times both. Read out by tools/run_sha_test.py.

   [0]    done
   [1]    blocks compared
   [2]    mismatches
   [3..4] frames for 8 C compressions
   [5..6] frames for 8 assembly compressions
   [7]    1 if the known BIP-39 vector still comes out right
   [8..71] the two digests of the first mismatching block, if any */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "crypto/sha512.h"
#include "crypto/hmac512.h"

uint8_t ts_out[80];

static volatile uint16_t frames;
static void tick(void) { frames++; }

static uint8_t buf[128];
static uint8_t dig_c[64], dig_a[64];
static uint16_t lcg;

static uint8_t rnd(void)
{
    lcg = (uint16_t)(lcg * 25173u + 13849u);
    return (uint8_t)(lcg >> 8);
}

void main(void)
{
    uint8_t round, i, bad = 0;
    uint16_t t_c = 0, t_a = 0;

    if (_cpu == CGB_TYPE) cpu_fast();
    for (i = 0; i < sizeof(ts_out); i++) ts_out[i] = 0;
    add_VBL(tick);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();
    DISPLAY_ON;

    lcg = 0xACE1;
    for (round = 0; round < 24; round++) {
        uint8_t len = (uint8_t)(round == 0 ? 0 : (round == 1 ? 111 : 64));
        for (i = 0; i < 128; i++) buf[i] = rnd();
        if (round == 2) for (i = 0; i < 128; i++) buf[i] = 0x00;
        if (round == 3) for (i = 0; i < 128; i++) buf[i] = 0xff;

        sha512_use_asm = 0;
        sha512(buf, len, dig_c);
        sha512_use_asm = 1;
        sha512(buf, len, dig_a);

        for (i = 0; i < 64; i++) {
            if (dig_c[i] != dig_a[i]) {
                if (!bad) {
                    uint8_t j;
                    for (j = 0; j < 32; j++) {
                        ts_out[8 + j] = dig_c[j];
                        ts_out[40 + j] = dig_a[j];
                    }
                }
                bad++;
                break;
            }
        }
        ts_out[1] = (uint8_t)(round + 1);
        ts_out[2] = bad;
    }

    /* timings: eight one-block hashes each way */
    sha512_use_asm = 0;
    frames = 0;
    for (i = 0; i < 8; i++) sha512(buf, 64, dig_c);
    t_c = frames;
    sha512_use_asm = 1;
    frames = 0;
    for (i = 0; i < 8; i++) sha512(buf, 64, dig_a);
    t_a = frames;

    ts_out[3] = (uint8_t)t_c; ts_out[4] = (uint8_t)(t_c >> 8);
    ts_out[5] = (uint8_t)t_a; ts_out[6] = (uint8_t)(t_a >> 8);

    /* the official BIP-39 seed for "abandon ... about", via the assembly */
    {
        static const char m[] =
            "abandon abandon abandon abandon abandon abandon abandon abandon "
            "abandon abandon abandon about";
        uint8_t seed[64];
        sha512_use_asm = 1;
        pbkdf2_sha512_64((const uint8_t *)m, (uint16_t)strlen(m),
                         (const uint8_t *)"mnemonic", 8, 64, seed);
        /* 64 rounds, not 2048: enough to exercise HMAC without a long wait.
           The host test covers the full 2048-round vector. */
        for (i = 0; i < 16; i++) ts_out[64 + i] = seed[i];
        ts_out[7] = 1;
    }

    ts_out[0] = 1;
    while (1) vsync();
}
