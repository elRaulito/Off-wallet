#ifdef OW_GAMEBOY
#pragma bank 4
#endif
/* SHA-512 for an 8-bit CPU.
 *
 * Words are eight little-endian bytes, not a pair of 32-bit halves. That
 * matters a lot here: SDCC turns a shift like `x >> 28` on a 32-bit value into
 * twenty-eight single-bit shifts, so the obvious hi/lo implementation spent
 * almost all its time shifting. Byte-wise, every SHA-512 rotation is a byte
 * permutation plus a shift of at most 7 bits:
 *
 *   rotr(x, 8q + r) : take byte (i+q) mod 8, then shift right r bits,
 *                     pulling in the low bits of the next byte.
 *
 * The first version of this file measured ~1.2 s per compression on hardware;
 * this one is far cheaper, which is what makes the BIP-39 mode bearable.
 */
#include "sha512.h"
#include "sha512_tables.h"

#define W64 8

#define U8(op) op(0) op(1) op(2) op(3) op(4) op(5) op(6) op(7)

static void w_copy(uint8_t *d, const uint8_t *s)
{
#define CP(i) d[i] = s[i];
    U8(CP)
#undef CP
}

static void w_add(uint8_t *d, const uint8_t *a, const uint8_t *b)
{
    uint16_t c = 0;
#define AD(i) c = (uint16_t)a[i] + (uint16_t)b[i] + (c >> 8); d[i] = (uint8_t)c;
    U8(AD)
#undef AD
}

static void w_add_to(uint8_t *d, const uint8_t *a)
{
    uint16_t c = 0;
#define W_AT(i) c = (uint16_t)d[i] + (uint16_t)a[i] + (c >> 8); d[i] = (uint8_t)c;
    U8(W_AT)
#undef W_AT
}

/* d = rotr(s, n), 1 <= n <= 63 */
static void w_rotr(uint8_t *d, const uint8_t *s, uint8_t n)
{
    uint8_t q = (uint8_t)(n >> 3);
    uint8_t r = (uint8_t)(n & 7);
    uint8_t lo, hi;

    uint8_t inv = (uint8_t)(8 - r);
    if (r == 0) {
#define RB(i) d[i] = s[(uint8_t)(i + q) & 7];
        U8(RB)
#undef RB
        return;
    }
#define RT(i) lo = s[(uint8_t)(i + q) & 7];                                     hi = s[(uint8_t)(i + q + 1) & 7];                                 d[i] = (uint8_t)((lo >> r) | (hi << inv));
    U8(RT)
#undef RT
}

/* d = s >> n (logical), 1 <= n <= 7 */
static void w_shr(uint8_t *d, const uint8_t *s, uint8_t n)
{
    uint8_t inv = (uint8_t)(8 - n);
#define SR(i) d[i] = (uint8_t)((s[i] >> n) | (s[i + 1] << inv));
    SR(0) SR(1) SR(2) SR(3) SR(4) SR(5) SR(6)
#undef SR
    d[7] = (uint8_t)(s[7] >> n);
}

static void w_xor_to(uint8_t *d, const uint8_t *a)
{
#define XT(i) d[i] ^= a[i];
    U8(XT)
#undef XT
}

/* Round working state kept in static storage: the Game Boy stack is small and
   SDCC addresses globals more cheaply than frame offsets. */
static uint8_t W[16][8];
static uint8_t sa[8], sb[8], sc_[8], sd[8], se[8], sf[8], sg[8], sh[8];
static uint8_t t1[8], t2[8], s0[8], s1[8], tmp[8], tmp2[8];

static void sha512_block_c(sha512_ctx *c, const uint8_t *p)
{
    uint8_t i;

    for (i = 0; i < 16; i++) {
        const uint8_t *q = p + i * 8;
#define LD(j) W[i][j] = q[7 - j];              /* message is big-endian */
        U8(LD)
#undef LD
    }

    w_copy(sa, c->h[0]); w_copy(sb, c->h[1]);
    w_copy(sc_, c->h[2]); w_copy(sd, c->h[3]);
    w_copy(se, c->h[4]); w_copy(sf, c->h[5]);
    w_copy(sg, c->h[6]); w_copy(sh, c->h[7]);

    for (i = 0; i < 80; i++) {
        uint8_t *wi = W[i & 15];

        if (i >= 16) {
            /* s0 = sigma0(W[i-15]), s1 = sigma1(W[i-2]) */
            uint8_t *x = W[(uint8_t)(i + 1) & 15];
            uint8_t *y = W[(uint8_t)(i + 14) & 15];
            w_rotr(s0, x, 1);
            w_rotr(tmp, x, 8);
            w_xor_to(s0, tmp);
            w_shr(tmp, x, 7);
            w_xor_to(s0, tmp);

            w_rotr(s1, y, 19);
            w_rotr(tmp, y, 61);
            w_xor_to(s1, tmp);
            w_shr(tmp, y, 6);
            w_xor_to(s1, tmp);

            w_add_to(wi, s0);
            w_add_to(wi, W[(uint8_t)(i + 9) & 15]);
            w_add_to(wi, s1);
        }

        /* Sum1(e) = rotr(e,14) ^ rotr(e,18) ^ rotr(e,41) */
        w_rotr(s1, se, 14);
        w_rotr(tmp, se, 18);
        w_xor_to(s1, tmp);
        w_rotr(tmp, se, 41);
        w_xor_to(s1, tmp);

        /* Ch(e,f,g) = (e & f) ^ (~e & g) */
#define CH(j) tmp[j] = (uint8_t)((se[j] & sf[j]) ^ ((uint8_t)~se[j] & sg[j]));
        U8(CH)
#undef CH

        w_add(t1, sh, s1);
        w_add_to(t1, tmp);
        w_add_to(t1, SHA512_K[i]);
        w_add_to(t1, wi);

        /* Sum0(a) = rotr(a,28) ^ rotr(a,34) ^ rotr(a,39) */
        w_rotr(s0, sa, 28);
        w_rotr(tmp, sa, 34);
        w_xor_to(s0, tmp);
        w_rotr(tmp, sa, 39);
        w_xor_to(s0, tmp);

        /* Maj(a,b,c) */
#define MJ(j) tmp2[j] = (uint8_t)((sa[j] & sb[j]) ^ (sa[j] & sc_[j])                                     ^ (sb[j] & sc_[j]));
        U8(MJ)
#undef MJ

        w_add(t2, s0, tmp2);

        w_copy(sh, sg); w_copy(sg, sf); w_copy(sf, se);
        w_add(se, sd, t1);
        w_copy(sd, sc_); w_copy(sc_, sb); w_copy(sb, sa);
        w_add(sa, t1, t2);
    }

    w_add_to(c->h[0], sa); w_add_to(c->h[1], sb);
    w_add_to(c->h[2], sc_); w_add_to(c->h[3], sd);
    w_add_to(c->h[4], se); w_add_to(c->h[5], sf);
    w_add_to(c->h[6], sg); w_add_to(c->h[7], sh);
}

#ifdef OW_GAMEBOY
/* The hand-written SM83 version (sha512_sm83.s, generated by
   tools/gen/gen_sha512_asm.py). It takes its arguments through two pointer
   variables it owns, and lives in this same bank so the call is direct.
   The switch is a variable rather than an #ifdef so one ROM can run both and
   compare them -- see src/testsha.c. */
extern uint8_t *sha_h;
extern uint8_t *sha_src;
void sha512_block_asm(void);

uint8_t sha512_use_asm = 1;

/* How many rounds the assembly runs. Always 80 in normal use; src/testsha1.c
   lowers it to inspect a single round. Defined here, not in the assembly,
   because an initialised byte must not be emitted into the RAM area. */
uint8_t sha_rounds = 80;

/* One compression in isolation, for src/testsha1.c. It has to be BANKED: the
   assembly lives in this bank, and a caller in another bank must come through
   the trampoline or it jumps into whatever is mapped at 0x4000. */
void sha512_compress_dbg(uint8_t *hstate, const uint8_t *block) OW_BANKED
{
    sha_h = hstate;
    sha_src = (uint8_t *)block;
    sha512_block_asm();
}

static void sha512_block(sha512_ctx *c, const uint8_t *p)
{
    if (sha512_use_asm) {
        sha_h = (uint8_t *)c->h;
        sha_src = (uint8_t *)p;
        sha512_block_asm();
    } else {
        sha512_block_c(c, p);
    }
}
#else
#define sha512_block(c, p) sha512_block_c((c), (p))
#endif

void sha512_init(sha512_ctx *c)
{
    uint8_t i;
    for (i = 0; i < 8; i++) w_copy(c->h[i], SHA512_H0[i]);
    c->buflen = 0;
    c->bitlen = 0;
    c->bitlen_hi = 0;
}

void sha512_update(sha512_ctx *c, const uint8_t *data, uint16_t len)
{
    uint8_t i;
    uint32_t old = c->bitlen;
    c->bitlen = old + ((uint32_t)len << 3);
    if (c->bitlen < old) c->bitlen_hi++;
    while (len) {
        if (c->buflen == 0 && len >= 128) {
            sha512_block(c, data);
            data += 128; len -= 128;
        } else {
            uint8_t space = (uint8_t)(128 - c->buflen);
            uint16_t n = (len < space) ? len : space;
            for (i = 0; i < (uint8_t)n; i++) c->buf[c->buflen + i] = data[i];
            c->buflen += (uint8_t)n;
            data += n; len -= n;
            if (c->buflen == 128) { sha512_block(c, c->buf); c->buflen = 0; }
        }
    }
}

void sha512_final(sha512_ctx *c, uint8_t out[64])
{
    uint8_t i, j;
    uint32_t lo = c->bitlen, hi = c->bitlen_hi;

    c->buf[c->buflen++] = 0x80;
    if (c->buflen > 112) {
        while (c->buflen < 128) c->buf[c->buflen++] = 0;
        sha512_block(c, c->buf);
        c->buflen = 0;
    }
    while (c->buflen < 120) c->buf[c->buflen++] = 0;
    c->buf[120] = (uint8_t)(hi >> 24); c->buf[121] = (uint8_t)(hi >> 16);
    c->buf[122] = (uint8_t)(hi >> 8);  c->buf[123] = (uint8_t)hi;
    c->buf[124] = (uint8_t)(lo >> 24); c->buf[125] = (uint8_t)(lo >> 16);
    c->buf[126] = (uint8_t)(lo >> 8);  c->buf[127] = (uint8_t)lo;
    sha512_block(c, c->buf);

    /* output is big-endian */
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            out[i * 8 + j] = c->h[i][7 - j];
}

void sha512(const uint8_t *data, uint16_t len, uint8_t out[64]) OW_BANKED
{
    sha512_ctx c;
    sha512_init(&c);
    sha512_update(&c, data, len);
    sha512_final(&c, out);
}
