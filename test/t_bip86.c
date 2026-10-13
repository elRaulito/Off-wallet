/* End-to-end check against the official BIP-86 test vectors:
   mnemonic -> BIP-39 seed -> m/86'/0'/0'/change/index -> bc1p address. */
#include <stdio.h>
#include <string.h>
#include "../src/crypto/hmac512.h"
#include "../src/crypto/wallet.h"
#include "../src/crypto/bech32.h"

static const char *MNEMONIC =
    "abandon abandon abandon abandon abandon abandon abandon abandon "
    "abandon abandon abandon about";

struct vec {
    uint32_t change, index;
    const char *internal_key;
    const char *output_key;
    const char *address;
};

/* From bips/bip-0086.mediawiki "Test vectors" */
static const struct vec V[] = {
    { 0, 0,
      "cc8a4bc64d897bddc5fbc2f670f7a8ba0b386779106cf1223c6fc5d7cd6fc115",
      "a60869f0dbcf1dc659c9cecbaf8050135ea9e8cdc487053f1dc6880949dc684c",
      "bc1p5cyxnuxmeuwuvkwfem96lqzszd02n6xdcjrs20cac6yqjjwudpxqkedrcr" },
    { 0, 1,
      "83dfe85a3151d2517290da461fe2815591ef69f2b18a2ce63f01697a8b313145",
      "a82f29944d65b86ae6b5e5cc75e294ead6c59391a1edc5e016e3498c67fc7bbb",
      "bc1p4qhjn9zdvkux4e44uhx8tc55attvtyu358kutcqkudyccelu0was9fqzwh" },
    { 1, 0,
      "399f1b2f4393f29a18c937859c5dd8a77350103157eb880f02e8c08214277cef",
      "882d74e5d0572d5a816cef0041a96b6c1de832f6f9676d9605c44d5e9a97d3dc",
      "bc1p3qkhfews2uk44qtvauqyr2ttdsw7svhkl9nkm9s9c3x4ax5h60wqwruhk7" },
};

static void hex(char *dst, const uint8_t *b, int n)
{
    for (int i = 0; i < n; i++) sprintf(dst + i * 2, "%02x", b[i]);
}

int main(void)
{
    uint8_t seed[64];
    char buf[80];
    int fails = 0;

    pbkdf2_sha512_64((const uint8_t *)MNEMONIC, (uint16_t)strlen(MNEMONIC),
                     (const uint8_t *)"mnemonic", 8, 2048, seed);
    hex(buf, seed, 64);
    printf("bip39 seed   %s\n", buf);

    for (unsigned v = 0; v < sizeof(V) / sizeof(V[0]); v++) {
        wallet_addr w;
        if (!wallet_derive_p2tr(&w, seed, 64, V[v].change, V[v].index)) {
            printf("m/86'/0'/0'/%u/%u  DERIVATION FAILED\n", V[v].change, V[v].index);
            fails++;
            continue;
        }
        printf("m/86'/0'/0'/%u/%u\n", V[v].change, V[v].index);
        hex(buf, w.internal_key, 32);
        printf("  internal  %s %s\n", buf,
               strcmp(buf, V[v].internal_key) ? (fails++, "FAIL") : "ok");
        hex(buf, w.output_key, 32);
        printf("  output    %s %s\n", buf,
               strcmp(buf, V[v].output_key) ? (fails++, "FAIL") : "ok");
        printf("  address   %s %s\n", w.address,
               strcmp(w.address, V[v].address) ? (fails++, "FAIL") : "ok");
    }

    /* bech32m sanity: BIP-350 valid P2TR example */
    {
        uint8_t prog[32];
        for (int i = 0; i < 32; i++) prog[i] = (uint8_t)i;
        segwit_addr_encode(buf, "bc", 1, prog, 32);
        printf("bech32m len %d -> %s\n", (int)strlen(buf), buf);
    }

    printf(fails ? "\n%d FAILURES\n" : "\nALL BIP86 VECTORS PASS\n", fails);
    return fails ? 1 : 0;
}
