#!/usr/bin/env bash
# Run every check: host differential tests against Python, the official BIP
# vectors, then the emulator tests on the real ROMs.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CRY="$ROOT/src/crypto"
HOST_SRC="$CRY/sha256.c $CRY/sha512.c $CRY/hmac512.c $CRY/secp256k1.c \
          $CRY/secp256k1_comb.c $CRY/secp256k1_consts.c $CRY/bech32.c $CRY/wallet.c \
          $CRY/bip39.c $CRY/bip39_words.c $CRY/base58.c"

echo "=== 1. regenerating tables (re-proves the curve constants) ==="
(cd tools/gen && python gen_secp_tables.py && python gen_wordlist.py)

echo
echo "=== 2. host: hashes against Python hashlib ==="
cd test
clang -O2 -std=c99 -o t_hash.exe t_hash.c $CRY/sha256.c $CRY/sha512.c $CRY/hmac512.c
./t_hash.exe > out_hash.txt
python - <<'PY'
import hashlib, hmac
got = dict(l.split() for l in open('out_hash.txt'))
exp = {
 'sha256_empty': hashlib.sha256(b'').hexdigest(),
 'sha256_abc': hashlib.sha256(b'abc').hexdigest(),
 'sha256_1000a': hashlib.sha256(b'a'*1000).hexdigest(),
 'sha512_empty': hashlib.sha512(b'').hexdigest(),
 'sha512_abc': hashlib.sha512(b'abc').hexdigest(),
 'sha512_1000a': hashlib.sha512(b'a'*1000).hexdigest(),
 'hmac512_key_fox': hmac.new(b'key', b'The quick brown fox jumps over the lazy dog', hashlib.sha512).hexdigest(),
 'pbkdf2_2048': hashlib.pbkdf2_hmac('sha512', b'password', b'salt', 2048, 64).hex(),
}
exp['sha256_1000a_bytewise'] = exp['sha256_1000a']
exp['sha512_1000a_bytewise'] = exp['sha512_1000a']
bad = [k for k, v in exp.items() if got.get(k) != v]
print('checked %d hash values, %d wrong' % (len(exp), len(bad)))
assert not bad, bad
PY

echo
echo "=== 3. host: field/curve arithmetic against Python bignums ==="
clang -O2 -std=c99 -o t_secp.exe t_secp.c $CRY/sha256.c $CRY/secp256k1.c \
      $CRY/secp256k1_comb.c $CRY/secp256k1_consts.c
./t_secp.exe > out_secp.txt
python check_secp.py out_secp.txt

echo
echo "=== 4. host: official BIP-86 test vectors ==="
clang -O2 -std=c99 -o t_bip86.exe t_bip86.c $HOST_SRC
./t_bip86.exe | tail -3

echo
echo "=== 5. host: BIP-39 mnemonic generation ==="
clang -O2 -std=c99 -o t_bip39.exe t_bip39.c $HOST_SRC
./t_bip39.exe | head -3

echo
echo "=== 6. host: WIF ==="
clang -O2 -std=c99 -o t_wif.exe t_wif.c $CRY/base58.c $CRY/sha256.c
./t_wif.exe | head -2
cd "$ROOT"

echo
echo "=== 7. hardware: assembly multiply vs C ==="
bash tools/build.sh testmul > /dev/null
python tools/run_mul_test.py 2>&1 | grep -vE "UserWarning|WARNING|INFO|###"

echo
echo "=== 8. hardware: field/curve stack + timings ==="
bash tools/build.sh testfe > /dev/null
python tools/run_fe_test.py 2>&1 | grep -vE "UserWarning|WARNING|INFO|###"

echo
echo "=== 9. hardware: the assembly SHA-512 against the C one ==="
bash tools/build.sh testsha > /dev/null
python tools/run_sha_test.py 2>&1 | grep -vE "UserWarning|WARNING|INFO|###"

echo
echo "=== 10. content: the ROM shows what content/story.txt says ==="
bash tools/build.sh game > /dev/null
python tools/run_content_test.py 2>&1 | grep -vE "UserWarning|WARNING|INFO|###"

echo
echo "=== 10. host: the QR encoder, decoded by OpenCV ==="
cd test
clang -O2 -std=c99 -o t_qr.exe t_qr.c "$CRY/../qr.c"
./t_qr.exe > /dev/null
python check_qr.py
cd "$ROOT"

echo
echo "=== 11. the game ROM, end to end ==="
bash tools/build.sh game > /dev/null
python tools/check_rom.py build/offwallet.gbc | tail -2
python tools/run_game_test.py swift 2>&1 | grep -vE "UserWarning|WARNING|INFO|###|Deprecation|screen_image" | tail -16

echo
echo "=== 12. the QR code, read off the emulated screen ==="
python tools/run_qr_test.py 2>&1 | grep -vE "UserWarning|WARNING|INFO|###"

echo
echo "=== 13. the UTXO list: typed in, stored, reloaded ==="
python tools/run_utxo_test.py 2>&1 | grep -vE "UserWarning|WARNING|INFO|###" | tail -12
