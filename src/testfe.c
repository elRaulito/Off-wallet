/* Hardware verification of the whole crypto stack, with timings.
   Field/curve vectors come from the host build, which check_secp.py verified
   against Python bignum arithmetic; the derivation is checked against the
   official BIP-86 vector. A pass here means the Game Boy build agrees with
   independently computed mathematics. */
#include <gb/gb.h>
#include <gb/cgb.h>

#include "crypto/secp256k1.h"
#include "crypto/sha512.h"
#include "crypto/hmac512.h"
#include "crypto/wallet.h"
#include "test_vectors.h"

/* [0] done, [1] pass bitmap, [2..] timings (frames, little-endian):
   0: 16 fe_mul  1: 16 fe_sqr  2: fe_inv  3: k*G  4: 8 sha512  5: full derive
   [16..23] first 8 bytes of the derived taproot output key */
uint8_t tf_out[32];

static volatile uint16_t frames;
static void tick(void) { frames++; }

/* official BIP-39 seed for "abandon ... about" */
static const uint8_t SEED[64] = {
  0x5e,0xb0,0x0b,0xbd,0xdc,0xf0,0x69,0x08,0x48,0x89,0xa8,0xab,0x91,0x55,0x56,0x81,
  0x65,0xf5,0xc4,0x53,0xcc,0xb8,0x5e,0x70,0x81,0x1a,0xae,0xd6,0xf6,0xda,0x5f,0xc1,
  0x9a,0x5a,0xc4,0x0b,0x38,0x9c,0xd3,0x70,0xd0,0x86,0x20,0x6d,0xec,0x8a,0xa6,0xc4,
  0x3d,0xae,0xa6,0x69,0x0f,0x20,0xad,0x3d,0x8d,0x48,0xb2,0xd2,0xce,0x9e,0x38,0xe4
};
/* expected output key for m/86'/0'/0'/0/0 from that seed (BIP-86 vector) */
static const uint8_t EXP_OKEY[8] = {0xa6,0x08,0x69,0xf0,0xdb,0xcf,0x1d,0xc6};

static uint8_t same(const uint8_t *a, const uint8_t *b, uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static void mark(uint8_t slot, uint16_t v)
{
    tf_out[2 + slot * 2] = (uint8_t)v;
    tf_out[3 + slot * 2] = (uint8_t)(v >> 8);
}

void main(void)
{
    fe r;
    ge_a P;
    uint8_t i, pass = 0;
    uint8_t h64[64];
    wallet_addr w;

    if (_cpu == CGB_TYPE) cpu_fast();
    for (i = 0; i < sizeof(tf_out); i++) tf_out[i] = 0;
    add_VBL(tick);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();
    DISPLAY_ON;

    /* ---- correctness ---- */
    fe_mul(r, TV_A, TV_B);
    if (same(r, TV_MUL, 32)) pass |= 0x01;
    fe_sqr(r, TV_A);
    if (same(r, TV_SQR, 32)) pass |= 0x02;
    fe_inv(r, TV_A);
    if (same(r, TV_INV, 32)) pass |= 0x04;
    if (secp_mul_g(&P, TV_K) && same(P.x, TV_KGX, 32) && same(P.y, TV_KGY, 32))
        pass |= 0x08;
    tf_out[1] = pass;

    /* ---- timings ---- */
    frames = 0;
    for (i = 0; i < 16; i++) fe_mul(r, TV_A, TV_B);
    mark(0, frames);

    frames = 0;
    for (i = 0; i < 16; i++) fe_sqr(r, TV_A);
    mark(1, frames);

    frames = 0;
    fe_inv(r, TV_A);
    mark(2, frames);

    frames = 0;
    secp_mul_g(&P, TV_K);
    mark(3, frames);

    /* 8 hashes of a 64-byte input: one compression block each plus padding */
    frames = 0;
    for (i = 0; i < 8; i++) sha512(SEED, 64, h64);
    mark(4, frames);

    /* full BIP-32 + taproot derivation from a known seed (no PBKDF2) */
    frames = 0;
    if (wallet_derive_p2tr(&w, SEED, 64, 0, 0)) {
        mark(5, frames);
        for (i = 0; i < 8; i++) tf_out[16 + i] = w.output_key[i];
        if (same(w.output_key, EXP_OKEY, 8)) pass |= 0x10;
    }
    tf_out[1] = pass;
    tf_out[0] = 1;
    while (1) vsync();
}
