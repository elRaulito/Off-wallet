#include "wallet.h"
#include "sha256.h"
#include "hmac512.h"
#include "secp256k1.h"
#include "bech32.h"

/* ---------------- BIP-32 ---------------- */

/* Master key from a BIP-39 seed: I = HMAC-SHA512("Bitcoin seed", seed) */
void bip32_master(bip32_node *out, const uint8_t *seed, uint16_t seedlen)
{
    uint8_t I[64];
    uint8_t i;
    hmac512((const uint8_t *)"Bitcoin seed", 12, seed, seedlen, I);
    bn_from_be(out->k, I);                       /* k as little-endian scalar */
    for (i = 0; i < 32; i++) out->c[i] = I[32 + i];
}

/* CKDpriv. index >= 0x80000000 means hardened.
   Returns 0 on the (negligible) invalid-child case. */
uint8_t bip32_ckd(bip32_node *out, const bip32_node *parent, uint32_t index)
{
    uint8_t data[37];
    uint8_t I[64];
    sc IL;
    uint8_t i;

    if (index & 0x80000000UL) {
        data[0] = 0;
        bn_to_be(data + 1, parent->k);
        i = 33;
    } else {
        ge_a P;
        if (!secp_mul_g(&P, parent->k)) return 0;
        secp_pubkey_compressed(data, &P);
        i = 33;
    }
    data[i++] = (uint8_t)(index >> 24);
    data[i++] = (uint8_t)(index >> 16);
    data[i++] = (uint8_t)(index >> 8);
    data[i++] = (uint8_t)(index);

    hmac512(parent->c, 32, data, i, I);
    bn_from_be(IL, I);
    if (!sc_is_valid(IL)) return 0;              /* IL must be in [1, n-1] */
    sc_add(out->k, IL, parent->k);
    if (bn_is_zero(out->k)) return 0;
    for (i = 0; i < 32; i++) out->c[i] = I[32 + i];
    return 1;
}

/* ---------------- BIP-86 / BIP-341 taproot ---------------- */

/* Single-key P2TR output key:
     internal = lift_x(x(P))              (the even-Y point with that x)
     t        = tagged_hash("TapTweak", x(internal))
     Q        = internal + t*G
     output   = x(Q)
   With no script tree the TapTweak message is just the 32-byte x-only key. */
uint8_t taproot_output_key(uint8_t out_be[32], const ge_a *pubkey)
{
    uint8_t xbe[32];
    uint8_t thash[32];
    sc t;
    ge_a internal;
    ge_j acc;
    ge_a Q;

    bn_copy(internal.x, pubkey->x);
    /* lift_x: keep the even-Y representative */
    if (fe_is_odd(pubkey->y)) fe_neg(internal.y, pubkey->y);
    else                      bn_copy(internal.y, pubkey->y);

    bn_to_be(xbe, internal.x);
    tagged_hash("TapTweak", xbe, 32, thash);
    bn_from_be(t, thash);
    if (!sc_is_valid(t)) return 0;

    /* t*G stays Jacobian and the internal key is added into it, so the whole
       tweak costs one inversion instead of two. */
    if (!secp_mul_g_j(&acc, t)) return 0;
    ge_add_affine(&acc, &acc, &internal);
    if (ge_is_infinity(&acc)) return 0;
    ge_to_affine(&Q, &acc);

    bn_to_be(out_be, Q.x);
    return 1;
}

/* ---------------- full derivation ---------------- */

/* One standalone private key -> P2TR address, the BIP-86 tweak with no
   script tree. Equivalent to the descriptor tr(<key>). */
uint8_t wallet_single_p2tr(wallet_addr *out, const uint8_t privkey_be[32])
{
    sc k;
    ge_a P;
    uint8_t i;

    bn_from_be(k, privkey_be);
    if (!sc_is_valid(k)) return 0;

    for (i = 0; i < 32; i++) out->privkey[i] = privkey_be[i];
    if (!secp_mul_g(&P, k)) return 0;
    bn_to_be(out->internal_key, P.x);
    if (!taproot_output_key(out->output_key, &P)) return 0;
    segwit_addr_encode(out->address, "bc", 1, out->output_key, 32);
    return 1;
}

/* seed -> m/86'/0'/0'/change/index -> bc1p... address */
uint8_t wallet_derive_p2tr(wallet_addr *out,
                           const uint8_t *seed, uint16_t seedlen,
                           uint32_t change, uint32_t index)
{
    bip32_node n, c;
    ge_a P;
    uint8_t i;

    bip32_master(&n, seed, seedlen);
    if (!bip32_ckd(&c, &n, 86UL | 0x80000000UL)) return 0;
    n = c;
    if (!bip32_ckd(&c, &n, 0UL | 0x80000000UL)) return 0;
    n = c;
    if (!bip32_ckd(&c, &n, 0UL | 0x80000000UL)) return 0;
    n = c;
    if (!bip32_ckd(&c, &n, change)) return 0;
    n = c;
    if (!bip32_ckd(&c, &n, index)) return 0;
    n = c;

    for (i = 0; i < 32; i++) out->privkey[i] = 0;
    bn_to_be(out->privkey, n.k);

    if (!secp_mul_g(&P, n.k)) return 0;
    bn_to_be(out->internal_key, P.x);
    if (!taproot_output_key(out->output_key, &P)) return 0;

    segwit_addr_encode(out->address, "bc", 1, out->output_key, 32);
    return 1;
}
