#include "secp256k1.h"
#include "secp256k1_tables.h"
#include "ow_bank.h"

/* Optional progress hook: the game points this at an animation updater so the
   screen keeps moving during the (slow) scalar multiplications. */
void (*secp_progress_cb)(void) = 0;

/* ===================== 256-bit helpers ===================== */

void bn_zero(uint8_t *r)
{
    uint8_t i;
    for (i = 0; i < 32; i++) r[i] = 0;
}

void bn_copy(uint8_t *r, const uint8_t *a)
{
    uint8_t i;
    for (i = 0; i < 32; i++) r[i] = a[i];
}

uint8_t bn_is_zero(const uint8_t *a)
{
    uint8_t i, acc = 0;
    for (i = 0; i < 32; i++) acc |= a[i];
    return (uint8_t)(acc == 0);
}

int8_t bn_cmp(const uint8_t *a, const uint8_t *b)
{
    uint8_t i = 32;
    while (i--) {
        if (a[i] != b[i]) return (a[i] > b[i]) ? 1 : -1;
    }
    return 0;
}

uint8_t bn_add(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
    uint16_t t = 0;
    uint8_t i;
    for (i = 0; i < 32; i++) {
        t = (uint16_t)a[i] + (uint16_t)b[i] + (t >> 8);
        r[i] = (uint8_t)t;
    }
    return (uint8_t)(t >> 8);
}

uint8_t bn_sub(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
    uint16_t t = 0;   /* borrow kept in bit 8 */
    uint8_t i;
    for (i = 0; i < 32; i++) {
        t = (uint16_t)a[i] - (uint16_t)b[i] - ((t >> 8) & 1);
        r[i] = (uint8_t)t;
    }
    return (uint8_t)((t >> 8) & 1);
}

void bn_from_be(uint8_t *r, const uint8_t be[32])
{
    uint8_t i;
    for (i = 0; i < 32; i++) r[i] = be[31 - i];
}

void bn_to_be(uint8_t be[32], const uint8_t *a)
{
    uint8_t i;
    for (i = 0; i < 32; i++) be[i] = a[31 - i];
}

/* ===================== field arithmetic mod p ===================== */

void fe_add(fe r, const fe a, const fe b)
{
    uint8_t carry = bn_add(r, a, b);
    if (carry || bn_cmp(r, SECP_P) >= 0) bn_sub(r, r, SECP_P);
    if (bn_cmp(r, SECP_P) >= 0) bn_sub(r, r, SECP_P);
}

void fe_sub(fe r, const fe a, const fe b)
{
    if (bn_sub(r, a, b)) bn_add(r, r, SECP_P);
}

void fe_neg(fe r, const fe a)
{
    if (bn_is_zero(a)) bn_zero(r);
    else bn_sub(r, SECP_P, a);
}

/* acc[0..len) += m[0..mlen) << (8*off) */
static void acc_add_shifted(uint8_t *acc, uint8_t len,
                            const uint8_t *m, uint8_t mlen, uint8_t off)
{
    uint16_t carry = 0;
    uint8_t i;
    for (i = 0; i < mlen; i++) {
        carry += (uint16_t)acc[off + i] + (uint16_t)m[i];
        acc[off + i] = (uint8_t)carry;
        carry >>= 8;
    }
    i = (uint8_t)(off + mlen);
    while (carry && i < len) {
        carry += acc[i];
        acc[i] = (uint8_t)carry;
        carry >>= 8;
        i++;
    }
}

/* acc[0..len) += m[0..mlen) * 977, as m*209 + (m*3 << 8).
   Both multipliers are compile-time constants, which SDCC expands into inline
   shift-and-add; a hand-rolled version built from six shifted array adds was
   measured slower (56 ms vs 37 ms per field multiply), so this stays. */
static void acc_add_mul_977(uint8_t *acc, uint8_t len,
                            const uint8_t *m, uint8_t mlen)
{
    uint16_t t;
    uint8_t carry = 0;
    uint8_t i;

    for (i = 0; i < mlen; i++) {
        t = (uint16_t)acc[i] + (uint16_t)m[i] * 209u + carry;
        acc[i] = (uint8_t)t;
        carry = (uint8_t)(t >> 8);
    }
    i = mlen;
    while (carry && i < len) {
        t = (uint16_t)acc[i] + carry;
        acc[i] = (uint8_t)t;
        carry = (uint8_t)(t >> 8);
        i++;
    }
    carry = 0;
    for (i = 0; i < mlen; i++) {
        t = (uint16_t)acc[i + 1] + (uint16_t)m[i] * 3u + carry;
        acc[i + 1] = (uint8_t)t;
        carry = (uint8_t)(t >> 8);
    }
    i = (uint8_t)(mlen + 1);
    while (carry && i < len) {
        t = (uint16_t)acc[i] + carry;
        acc[i] = (uint8_t)t;
        carry = (uint8_t)(t >> 8);
        i++;
    }
}

/* Reduce the 512-bit product t (64 bytes LE) mod p, using
   2^256 == 2^32 + 977  (mod p). */
static void fe_reduce512(fe r, uint8_t *t)
{
    uint8_t acc[40];
    uint8_t hi[32];
    uint8_t i, nz;

    for (i = 0; i < 32; i++) { acc[i] = t[i]; hi[i] = t[32 + i]; }
    for (i = 32; i < 40; i++) acc[i] = 0;
    acc_add_mul_977(acc, 40, hi, 32);
    acc_add_shifted(acc, 40, hi, 32, 4);

    for (;;) {
        nz = 0;
        for (i = 32; i < 40; i++) nz |= acc[i];
        if (!nz) break;
        {
            uint8_t h[8];
            for (i = 0; i < 8; i++) { h[i] = acc[32 + i]; acc[32 + i] = 0; }
            acc_add_mul_977(acc, 40, h, 8);
            acc_add_shifted(acc, 40, h, 8, 4);
        }
    }

    for (i = 0; i < 32; i++) r[i] = acc[i];
    while (bn_cmp(r, SECP_P) >= 0) bn_sub(r, r, SECP_P);
}

#ifdef OW_GAMEBOY
/* On hardware the 256x256 product is done by hand-written SM83 assembly
   (fe_mul_sm83.s): 24 ms against 167 ms for the C loop below, because the CPU
   has no multiply instruction and SDCC calls a helper for every 8x8 product. */
extern uint8_t mul_a[32], mul_b[32], mul_t[68];
void fe_mul_core(void);

void fe_mul(fe r, const fe a, const fe b)
{
    uint8_t i;
    for (i = 0; i < 32; i++) mul_a[i] = a[i];
    for (i = 0; i < 32; i++) mul_b[i] = b[i];
    fe_mul_core();
    fe_reduce512(r, mul_t);
}
#else
void fe_mul(fe r, const fe a, const fe b)
{
    uint8_t t[64];
    uint8_t i, j, carry, ai;
    uint16_t acc;

    for (i = 0; i < 64; i++) t[i] = 0;
    for (i = 0; i < 32; i++) {
        ai = a[i];
        if (ai == 0) continue;
        carry = 0;
        for (j = 0; j < 32; j++) {
            /* 255 + 255*255 + 255 == 65535: fits uint16 exactly, no 32-bit math */
            acc = (uint16_t)t[i + j] + (uint16_t)ai * (uint16_t)b[j] + carry;
            t[i + j] = (uint8_t)acc;
            carry = (uint8_t)(acc >> 8);
        }
        t[i + 32] = carry;   /* t[i+32] is still 0 at this point */
    }
    fe_reduce512(r, t);
}
#endif

void fe_sqr(fe r, const fe a)
{
    fe_mul(r, a, a);
}

/* r = a^(p-2) mod p, the modular inverse for a != 0.
   Addition chain from libsecp256k1: 255 squarings + 15 multiplications,
   against 255 squarings + ~255 multiplications for plain square-and-multiply.
   The temporaries are static rather than on the stack: 384 bytes of Game Boy
   stack is a lot, the function is not recursive, and WRAM access is cheaper. */
static fe iv_x2, iv_x3, iv_x6, iv_x9, iv_x11, iv_x22, iv_x44, iv_x88,
          iv_x176, iv_x220, iv_x223, iv_t1;

static void fe_sqr_n(fe r, const fe a, uint8_t n)
{
    uint8_t i;
    fe_sqr(r, a);
    for (i = 1; i < n; i++) fe_sqr(r, r);
}

void fe_inv(fe r, const fe a)
{
    fe_sqr(iv_x2, a);
    fe_mul(iv_x2, iv_x2, a);              /* a^3      */
    fe_sqr(iv_x3, iv_x2);
    fe_mul(iv_x3, iv_x3, a);              /* a^7      */

    fe_sqr_n(iv_x6, iv_x3, 3);
    fe_mul(iv_x6, iv_x6, iv_x3);          /* 6 ones   */
    fe_sqr_n(iv_x9, iv_x6, 3);
    fe_mul(iv_x9, iv_x9, iv_x3);          /* 9 ones   */
    fe_sqr_n(iv_x11, iv_x9, 2);
    fe_mul(iv_x11, iv_x11, iv_x2);        /* 11 ones  */
    fe_sqr_n(iv_x22, iv_x11, 11);
    fe_mul(iv_x22, iv_x22, iv_x11);       /* 22 ones  */
    fe_sqr_n(iv_x44, iv_x22, 22);
    fe_mul(iv_x44, iv_x44, iv_x22);       /* 44 ones  */
    fe_sqr_n(iv_x88, iv_x44, 44);
    fe_mul(iv_x88, iv_x88, iv_x44);       /* 88 ones  */
    fe_sqr_n(iv_x176, iv_x88, 88);
    fe_mul(iv_x176, iv_x176, iv_x88);     /* 176 ones */
    fe_sqr_n(iv_x220, iv_x176, 44);
    fe_mul(iv_x220, iv_x220, iv_x44);     /* 220 ones */
    fe_sqr_n(iv_x223, iv_x220, 3);
    fe_mul(iv_x223, iv_x223, iv_x3);      /* 223 ones */

    fe_sqr_n(iv_t1, iv_x223, 23);
    fe_mul(iv_t1, iv_t1, iv_x22);
    fe_sqr_n(iv_t1, iv_t1, 5);
    fe_mul(iv_t1, iv_t1, a);
    fe_sqr_n(iv_t1, iv_t1, 3);
    fe_mul(iv_t1, iv_t1, iv_x2);
    fe_sqr_n(iv_t1, iv_t1, 2);
    fe_mul(r, a, iv_t1);
}

uint8_t fe_is_odd(const fe a)
{
    return (uint8_t)(a[0] & 1);
}

/* ===================== scalar arithmetic mod n ===================== */

uint8_t sc_is_valid(const sc a)
{
    if (bn_is_zero(a)) return 0;
    return (uint8_t)(bn_cmp(a, SECP_N) < 0);
}

void sc_add(sc r, const sc a, const sc b)
{
    uint8_t carry = bn_add(r, a, b);
    if (carry || bn_cmp(r, SECP_N) >= 0) bn_sub(r, r, SECP_N);
}

/* ===================== group operations ===================== */

void ge_set_infinity(ge_j *r)
{
    bn_zero(r->X); bn_zero(r->Y); bn_zero(r->Z);
    r->X[0] = 1; r->Y[0] = 1;
}

uint8_t ge_is_infinity(const ge_j *r)
{
    return bn_is_zero(r->Z);
}

/* dbl-2009-l for a = 0: 2M + 5S.
   Safe when r == a: Z3 is computed from a->Y/a->Z before any store into r. */
void ge_dbl(ge_j *r, const ge_j *a)
{
    fe A, B, C, D, E, F, t, Z3;

    if (ge_is_infinity(a)) { ge_set_infinity(r); return; }

    fe_mul(t, a->Y, a->Z);
    fe_add(Z3, t, t);                /* Z3 = 2*Y1*Z1, before r is touched */

    fe_sqr(A, a->X);
    fe_sqr(B, a->Y);
    fe_sqr(C, B);
    fe_add(t, a->X, B);
    fe_sqr(t, t);
    fe_sub(t, t, A);
    fe_sub(t, t, C);
    fe_add(D, t, t);
    fe_add(E, A, A);
    fe_add(E, E, A);
    fe_sqr(F, E);

    fe_add(t, D, D);
    fe_sub(r->X, F, t);

    fe_sub(t, D, r->X);
    fe_mul(t, E, t);
    fe_add(C, C, C);
    fe_add(C, C, C);
    fe_add(C, C, C);
    fe_sub(r->Y, t, C);

    bn_copy(r->Z, Z3);
}

/* madd-2007-bl: Jacobian + affine -> Jacobian */
void ge_add_affine(ge_j *r, const ge_j *a, const ge_a *b)
{
    fe Z1Z1, U2, S2, H, HH, I, J, rr, V, t, t2;

    if (ge_is_infinity(a)) {
        bn_copy(r->X, b->x);
        bn_copy(r->Y, b->y);
        bn_zero(r->Z); r->Z[0] = 1;
        return;
    }

    fe_sqr(Z1Z1, a->Z);
    fe_mul(U2, b->x, Z1Z1);
    fe_mul(S2, b->y, a->Z);
    fe_mul(S2, S2, Z1Z1);
    fe_sub(H, U2, a->X);
    fe_sub(t, S2, a->Y);
    fe_add(rr, t, t);

    if (bn_is_zero(H)) {
        if (bn_is_zero(rr)) { ge_dbl(r, a); return; }
        ge_set_infinity(r);
        return;
    }

    fe_sqr(HH, H);
    fe_add(I, HH, HH);
    fe_add(I, I, I);
    fe_mul(J, H, I);
    fe_mul(V, a->X, I);

    fe_sqr(t, rr);
    fe_sub(t, t, J);
    fe_add(t2, V, V);
    fe_sub(r->X, t, t2);

    fe_sub(t, V, r->X);
    fe_mul(t, rr, t);
    fe_mul(t2, a->Y, J);
    fe_add(t2, t2, t2);
    fe_sub(r->Y, t, t2);

    fe_add(t, a->Z, H);
    fe_sqr(t, t);
    fe_sub(t, t, Z1Z1);
    fe_sub(r->Z, t, HH);
}

void ge_to_affine(ge_a *r, const ge_j *a)
{
    fe zinv, z2;
    if (ge_is_infinity(a)) { bn_zero(r->x); bn_zero(r->y); return; }
    fe_inv(zinv, a->Z);
    fe_sqr(z2, zinv);
    fe_mul(r->x, a->X, z2);
    fe_mul(z2, z2, zinv);
    fe_mul(r->y, a->Y, z2);
}

/* r = k*G via the Lim-Lee comb: 32 doublings, each followed by one table
   addition. Bit 32*j + i of k contributes bit j of the table index, so the
   byte m selects the precomputed sum of teeth. */
/* k*G left in Jacobian form: lets a caller add another point before paying
   for the single modular inversion that converting to affine costs. */
uint8_t secp_mul_g_j(ge_j *out, const sc k)
{
    ge_j acc;
    ge_a tp;
    uint8_t i, j, m, sh, byi;
    const uint8_t *p;
    OW_BANK_SAVE();

    ge_set_infinity(&acc);
    i = SECP_COMB_SPACING;
    while (i--) {
        if (secp_progress_cb) secp_progress_cb();
        ge_dbl(&acc, &acc);
        sh  = (uint8_t)(i & 7);
        byi = (uint8_t)(i >> 3);
        m = 0;
        for (j = 0; j < SECP_COMB_TEETH; j++) {
            /* bit (32*j + i) of k lives in byte 4*j + (i>>3), bit i&7 */
            m |= (uint8_t)(((k[(uint8_t)(j * 4) + byi] >> sh) & 1) << j);
        }
        if (!m) continue;
        p = SECP_COMB_PTR((uint16_t)(m - 1) * 64);
        for (j = 0; j < 32; j++) { tp.x[j] = p[j]; tp.y[j] = p[32 + j]; }
        ge_add_affine(&acc, &acc, &tp);
    }
    OW_BANK_RESTORE();
    *out = acc;
    return (uint8_t)(!ge_is_infinity(&acc));
}

uint8_t secp_mul_g(ge_a *r, const sc k)
{
    ge_j j;
    if (!secp_mul_g_j(&j, k)) { bn_zero(r->x); bn_zero(r->y); return 0; }
    ge_to_affine(r, &j);
    return 1;
}

void secp_pubkey_compressed(uint8_t out[33], const ge_a *p)
{
    out[0] = fe_is_odd(p->y) ? 0x03 : 0x02;
    bn_to_be(out + 1, p->x);
}
