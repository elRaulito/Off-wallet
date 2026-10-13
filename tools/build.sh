#!/usr/bin/env bash
# Build OffWallet ROMs with GBDK-2020.
#   tools/build.sh selftest   -> build/selftest.gbc
#   tools/build.sh game       -> build/offwallet.gbc
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export GBDKDIR="$ROOT/tools/gbdk/"
LCC="$GBDKDIR/bin/lcc"
OUT="$ROOT/build"
mkdir -p "$OUT"

TARGET="${1:-game}"

# Crypto core lives in bank 0 (always mapped); big tables get their own banks.
CRYPTO="src/crypto/fe_mul_sm83.s src/crypto/sha512_sm83.s src/crypto/sha256.c src/crypto/sha512.c \
        src/crypto/hmac512.c src/crypto/secp256k1.c src/crypto/secp256k1_consts.c \
        src/crypto/bech32.c src/crypto/wallet.c src/crypto/bip39.c  src/crypto/base58.c"
BANKED="src/crypto/secp256k1_comb.c src/crypto/bip39_words.c"

# -Wm-yC       : CGB-only ROM
# -Wl-yt0x1B   : MBC5 + RAM + battery (save the wallet in SRAM)
# -Wl-yo8      : 8 ROM banks (128 KB)
# -Wl-ya1      : 1 RAM bank (8 KB)
# --max-allocs-per-node: let SDCC work harder on the crypto inner loops
CFLAGS="-DOW_GAMEBOY -Isrc -Wf--max-allocs-per-node8000 -Wf--opt-code-speed"
LDFLAGS="-Wl-j -Wl-m -Wm-yC -Wl-yt0x1B -Wl-yo8 -Wl-ya1 -Wm-ynOffWallet"

cd "$ROOT"

# Content (screens, dialogue, characters) is compiled from content/story.txt.
# Doing it here means editing the text file and rebuilding is a single step,
# and that content mistakes stop the build with a readable message.
if [ "$TARGET" = "game" ]; then
  python tools/gen/gen_story.py || exit 1
fi

case "$TARGET" in
  selftest) SRC="src/selftest.c $CRYPTO $BANKED"; NAME=selftest ;;
  bench)    SRC="src/bench.c $CRYPTO $BANKED"; NAME=bench ;;
  testmul)  SRC="src/testmul.c src/crypto/fe_mul_sm83.s"; NAME=testmul ;;
  testsha1) SRC="src/testsha1.c $CRYPTO $BANKED"; NAME=testsha1 ;;
  testsha)  SRC="src/testsha.c $CRYPTO $BANKED"; NAME=testsha ;;
  testfe)   SRC="src/testfe.c $CRYPTO $BANKED"; NAME=testfe ;;
  game)     SRC="src/main.c src/game.c src/story.c src/story_data.c src/story_tiles.c src/ui.c src/rng.c src/progress.c src/qr.c src/qr_draw.c src/utxo.c $CRYPTO $BANKED                  src/gfx/font_tiles.c src/gfx/gfx_tiles.c"; NAME=offwallet ;;
  *) echo "usage: $0 [selftest|game]" >&2; exit 1 ;;
esac

echo "building $NAME.gbc ..."
$LCC $CFLAGS $LDFLAGS -o "$OUT/$NAME.gbc" $SRC
ls -la "$OUT/$NAME.gbc"
# The .noi file carries symbol addresses; the test harness needs st_out's.
if [ -f "$OUT/$NAME.noi" ]; then
  grep -iE "DEF _st_out|DEF _frames" "$OUT/$NAME.noi" || true
fi
"$GBDKDIR/bin/romusage" "$OUT/$NAME.gbc" -g 2>/dev/null | head -30 || true
