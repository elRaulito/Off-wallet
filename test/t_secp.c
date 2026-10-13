/* Host-side differential test: prints field/group results for deterministic
   SHA-256-derived inputs; check_secp.py recomputes them with Python bignums. */
#include <stdio.h>
#include <string.h>
#include "../src/crypto/sha256.h"
#include "../src/crypto/secp256k1.h"
#include "../src/crypto/secp256k1_tables.h"

static void ph(const char *label, int idx, const uint8_t *le)
{
    uint8_t be[32];
    bn_to_be(be, le);
    printf("%s %d ", label, idx);
    for (int i = 0; i < 32; i++) printf("%02x", be[i]);
    printf("\n");
}

static void derive(uint8_t out[32], uint32_t seed)
{
    uint8_t in[4];
    in[0] = (uint8_t)(seed >> 24); in[1] = (uint8_t)(seed >> 16);
    in[2] = (uint8_t)(seed >> 8);  in[3] = (uint8_t)seed;
    uint8_t h[32];
    sha256(in, 4, h);
    bn_from_be(out, h);            /* interpret hash as a little-endian fe */
}

int main(void)
{
    fe a, b, r;
    sc k;
    ge_a P;
    uint8_t pub[33];

    /* field arithmetic */
    for (int i = 0; i < 40; i++) {
        derive(a, (uint32_t)i);
        derive(b, (uint32_t)i + 1000);
        /* reduce inputs into the field so Python and C agree on the operands */
        while (bn_cmp(a, SECP_P) >= 0) bn_sub(a, a, SECP_P);
        while (bn_cmp(b, SECP_P) >= 0) bn_sub(b, b, SECP_P);
        ph("A", i, a);
        ph("B", i, b);
        fe_add(r, a, b); ph("ADD", i, r);
        fe_sub(r, a, b); ph("SUB", i, r);
        fe_mul(r, a, b); ph("MUL", i, r);
        fe_sqr(r, a);    ph("SQR", i, r);
        fe_inv(r, a);    ph("INV", i, r);
        fe_neg(r, a);    ph("NEG", i, r);
    }

    /* scalar mod n */
    for (int i = 0; i < 10; i++) {
        derive(a, (uint32_t)i + 5000);
        derive(b, (uint32_t)i + 6000);
        while (bn_cmp(a, SECP_N) >= 0) bn_sub(a, a, SECP_N);
        while (bn_cmp(b, SECP_N) >= 0) bn_sub(b, b, SECP_N);
        ph("SA", i, a);
        ph("SB", i, b);
        sc_add(r, a, b); ph("SCADD", i, r);
    }

    /* fixed-base scalar multiplication */
    bn_zero(k); k[0] = 1;
    secp_mul_g(&P, k);
    printf("G1 ");
    for (int i = 0; i < 33; i++) printf("%02x", (secp_pubkey_compressed(pub, &P), pub[i]));
    printf("\n");
    ph("G1X", 0, P.x);
    ph("G1Y", 0, P.y);

    for (int i = 0; i < 12; i++) {
        derive(k, (uint32_t)i + 9000);
        while (bn_cmp(k, SECP_N) >= 0) bn_sub(k, k, SECP_N);
        ph("K", i, k);
        if (!secp_mul_g(&P, k)) { printf("MULG %d INFINITY\n", i); continue; }
        ph("MULGX", i, P.x);
        ph("MULGY", i, P.y);
        secp_pubkey_compressed(pub, &P);
        printf("PUB %d ", i);
        for (int j = 0; j < 33; j++) printf("%02x", pub[j]);
        printf("\n");
    }

    /* generic point addition through the Jacobian path: (k1+k2)*G == k1*G + k2*G */
    for (int i = 0; i < 6; i++) {
        sc k1, k2, ks;
        ge_a P1, P2, Ps;
        ge_j acc;
        derive(k1, (uint32_t)i + 12000);
        derive(k2, (uint32_t)i + 13000);
        while (bn_cmp(k1, SECP_N) >= 0) bn_sub(k1, k1, SECP_N);
        while (bn_cmp(k2, SECP_N) >= 0) bn_sub(k2, k2, SECP_N);
        secp_mul_g(&P1, k1);
        secp_mul_g(&P2, k2);
        sc_add(ks, k1, k2);
        secp_mul_g(&Ps, ks);
        /* acc = P1 (as Jacobian, Z=1), then add P2 */
        bn_copy(acc.X, P1.x); bn_copy(acc.Y, P1.y);
        bn_zero(acc.Z); acc.Z[0] = 1;
        ge_add_affine(&acc, &acc, &P2);
        ge_a Sum;
        ge_to_affine(&Sum, &acc);
        printf("ADDSAME %d %s\n", i,
               (bn_cmp(Sum.x, Ps.x) == 0 && bn_cmp(Sum.y, Ps.y) == 0) ? "OK" : "MISMATCH");
        /* doubling: 2*(k1*G) == (2*k1)*G */
        sc two, k2x;
        bn_zero(two); two[0] = 2;
        sc_add(k2x, k1, k1);
        secp_mul_g(&Ps, k2x);
        bn_copy(acc.X, P1.x); bn_copy(acc.Y, P1.y);
        bn_zero(acc.Z); acc.Z[0] = 1;
        ge_dbl(&acc, &acc);
        ge_to_affine(&Sum, &acc);
        printf("DBLSAME %d %s\n", i,
               (bn_cmp(Sum.x, Ps.x) == 0 && bn_cmp(Sum.y, Ps.y) == 0) ? "OK" : "MISMATCH");
        (void)two;
    }
    return 0;
}
