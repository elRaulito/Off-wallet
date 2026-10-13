#include <stdio.h>
#include <string.h>
#include "../src/crypto/sha256.h"
#include "../src/crypto/sha512.h"
#include "../src/crypto/hmac512.h"

static void ph(const char *label, const uint8_t *b, int n)
{
    printf("%s ", label);
    for (int i = 0; i < n; i++) printf("%02x", b[i]);
    printf("\n");
}

int main(void)
{
    uint8_t o32[32], o64[64];
    uint8_t buf[1000];

    sha256((const uint8_t*)"", 0, o32);          ph("sha256_empty", o32, 32);
    sha256((const uint8_t*)"abc", 3, o32);       ph("sha256_abc", o32, 32);
    for (int i = 0; i < 1000; i++) buf[i] = 'a';
    sha256(buf, 1000, o32);                      ph("sha256_1000a", o32, 32);
    /* multi-block via chunked update */
    { sha256_ctx c; sha256_init(&c);
      for (int i = 0; i < 1000; i++) sha256_update(&c, buf+i, 1);
      sha256_final(&c, o32); ph("sha256_1000a_bytewise", o32, 32); }
    sha256d((const uint8_t*)"hello", 5, o32);    ph("sha256d_hello", o32, 32);
    tagged_hash("TapTweak", (const uint8_t*)"abc", 3, o32); ph("tagged_TapTweak_abc", o32, 32);

    sha512((const uint8_t*)"", 0, o64);          ph("sha512_empty", o64, 64);
    sha512((const uint8_t*)"abc", 3, o64);       ph("sha512_abc", o64, 64);
    sha512(buf, 1000, o64);                      ph("sha512_1000a", o64, 64);
    { sha512_ctx c; sha512_init(&c);
      for (int i = 0; i < 1000; i++) sha512_update(&c, buf+i, 1);
      sha512_final(&c, o64); ph("sha512_1000a_bytewise", o64, 64); }

    hmac512((const uint8_t*)"key", 3, (const uint8_t*)"The quick brown fox jumps over the lazy dog", 43, o64);
    ph("hmac512_key_fox", o64, 64);
    hmac512(buf, 200, (const uint8_t*)"msg", 3, o64);   /* key > blocksize */
    ph("hmac512_longkey_msg", o64, 64);
    hmac512((const uint8_t*)"Bitcoin seed", 12, (const uint8_t*)"\x00\x01\x02\x03", 4, o64);
    ph("hmac512_bitcoinseed", o64, 64);

    pbkdf2_sha512_64((const uint8_t*)"password", 8, (const uint8_t*)"salt", 4, 1, o64);
    ph("pbkdf2_1", o64, 64);
    pbkdf2_sha512_64((const uint8_t*)"password", 8, (const uint8_t*)"salt", 4, 2048, o64);
    ph("pbkdf2_2048", o64, 64);
    /* BIP39: mnemonic "abandon...about", empty passphrase -> salt "mnemonic" */
    {
      const char *m = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
      pbkdf2_sha512_64((const uint8_t*)m, strlen(m), (const uint8_t*)"mnemonic", 8, 2048, o64);
      ph("bip39_seed_abandon", o64, 64);
    }
    return 0;
}
