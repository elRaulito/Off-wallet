#ifndef OW_SECP256K1_H
#define OW_SECP256K1_H
#include <stdint.h>

/* Called ~32 times per scalar multiplication when non-NULL. */
extern void (*secp_progress_cb)(void);

/* All 256-bit integers are 32 bytes, LITTLE-endian (limb 0 = least significant). */
typedef uint8_t fe[32];
typedef uint8_t sc[32];

typedef struct { fe X, Y, Z; } ge_j;   /* Jacobian; Z == 0 means point at infinity */
typedef struct { fe x, y; } ge_a;      /* affine */

/* --- 256-bit helpers (no modular reduction) --- */
void bn_zero(uint8_t *r);
void bn_copy(uint8_t *r, const uint8_t *a);
uint8_t bn_is_zero(const uint8_t *a);
int8_t  bn_cmp(const uint8_t *a, const uint8_t *b);   /* -1,0,1 */
uint8_t bn_add(uint8_t *r, const uint8_t *a, const uint8_t *b);  /* returns carry */
uint8_t bn_sub(uint8_t *r, const uint8_t *a, const uint8_t *b);  /* returns borrow */
void bn_from_be(uint8_t *r, const uint8_t be[32]);
void bn_to_be(uint8_t be[32], const uint8_t *a);

/* --- field arithmetic mod p --- */
void fe_add(fe r, const fe a, const fe b);
void fe_sub(fe r, const fe a, const fe b);
void fe_neg(fe r, const fe a);
void fe_mul(fe r, const fe a, const fe b);
void fe_sqr(fe r, const fe a);
void fe_inv(fe r, const fe a);
uint8_t fe_is_odd(const fe a);

/* --- scalar arithmetic mod n --- */
uint8_t sc_is_valid(const sc a);                 /* 0 < a < n */
void sc_add(sc r, const sc a, const sc b);       /* (a+b) mod n */

/* --- group operations --- */
void ge_set_infinity(ge_j *r);
uint8_t ge_is_infinity(const ge_j *r);
void ge_dbl(ge_j *r, const ge_j *a);
void ge_add_affine(ge_j *r, const ge_j *a, const ge_a *b);
void ge_to_affine(ge_a *r, const ge_j *a);

/* k*G using the pre-computed comb table; returns 0 if k == 0 (mod n). */
uint8_t secp_mul_g(ge_a *r, const sc k);
/* Same, but left in Jacobian coordinates: saves a modular inversion when the
   caller still has a point to add. */
uint8_t secp_mul_g_j(ge_j *out, const sc k);

/* Compressed SEC1 pubkey (33 bytes, big-endian X plus parity prefix). */
void secp_pubkey_compressed(uint8_t out[33], const ge_a *p);

#endif
