/* Self-test ROM: runs the real crypto on real Game Boy hardware and leaves the
   results in a fixed WRAM buffer so tools/run_selftest.py can read them out of
   an emulator and compare against the official BIP-86 vectors. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include <string.h>

#include "crypto/sha256.h"
#include "crypto/sha512.h"
#include "crypto/hmac512.h"
#include "crypto/secp256k1.h"
#include "crypto/wallet.h"
#include "crypto/bip39.h"

/* Layout the test harness reads:
     [0]      status: 0 = running, 1 = done
     [1]      stage counter, bumped as each step completes
     [2..3]   elapsed VBlank frames (little-endian)
     [4..35]  sha256("abc")
     [36..99] bip39 seed
     [100..131] taproot output key
     [132..195] address string
*/
uint8_t st_out[196];

static uint16_t frames;
static void tick(void) { frames++; }

static const char *MNEMONIC_12 =
    "abandon abandon abandon abandon abandon abandon abandon abandon "
    "abandon abandon abandon about";

void main(void)
{
    uint8_t seed[64];
    uint8_t h[32];
    wallet_addr w;
    uint16_t i;

    /* CGB double speed: the crypto is pure CPU work, so this halves the wait */
    if (_cpu == CGB_TYPE) cpu_fast();

    for (i = 0; i < sizeof(st_out); i++) st_out[i] = 0;

    add_VBL(tick);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();
    DISPLAY_ON;

    printf("OffWallet selftest\n\n");

    printf("sha256...\n");
    sha256((const uint8_t *)"abc", 3, h);
    for (i = 0; i < 32; i++) st_out[4 + i] = h[i];
    st_out[1] = 1;

    printf("bip39 seed\n(2048 rounds)...\n");
    frames = 0;
    bip39_seed(seed, MNEMONIC_12, (uint8_t)strlen(MNEMONIC_12));
    printf(" %u frames\n", frames);
    for (i = 0; i < 64; i++) st_out[36 + i] = seed[i];
    st_out[1] = 2;
    st_out[2] = (uint8_t)frames;
    st_out[3] = (uint8_t)(frames >> 8);

    printf("deriving\nm/86'/0'/0'/0/0...\n");
    frames = 0;
    if (wallet_derive_p2tr(&w, seed, 64, 0, 0)) {
        st_out[1] = 3;
        for (i = 0; i < 32; i++) st_out[100 + i] = w.output_key[i];
        for (i = 0; i < 63 && w.address[i]; i++) st_out[132 + i] = (uint8_t)w.address[i];
        printf(" %u frames\n", frames);
        printf("%s\n", w.address);
    } else {
        printf(" FAILED\n");
    }

    st_out[0] = 1;            /* done marker, written last */

    while (1) vsync();
}
