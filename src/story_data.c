/* AUTO-GENERATED from content/story.txt by tools/gen/gen_story.py.
   Edit the content file, not this one. */
#pragma bank 1
#include <gb/gb.h>
#include <gb/cgb.h>
#include "story.h"
#include "ui.h"
#include "gfx/gfx_tiles.h"

const story_char STORY_CHARS[1] = {
  { "SATOSHI",             STORY_TILE_BASE + 20,    7,  9,  2, 0x22 },   /* prof */
};

const story_op STORY_OPS[99] = {
  { SOP_SPRITE,  9,  3, 3, 0 },   /* btc */
  { SOP_TEXT,    1,  6, 0, "O F F W A L L E T" },
  { SOP_TEXT,    2,  8, 0, "a taproot wallet" },
  { SOP_TEXT,    1,  9, 0, "for game boy color" },
  { SOP_TEXT,    4, 13, 2, "PRESS START" },
  { SOP_TEXT,    1, 16, 0, "no net. no radio." },
  { SOP_TEXT,    1, 17, 0, "no way in." },
  { SOP_TEXT,    2,  1, 2, "CHOOSE A STARTER" },
  { SOP_SPRITE,  3,  4, 5, 0 },   /* egg_btc */
  { SOP_SPRITE,  9,  4, 4, 0 },   /* egg_b */
  { SOP_SPRITE, 15,  4, 6, 0 },   /* egg_c */
  { SOP_TEXT,    3,  7, 0, "BTC" },
  { SOP_TEXT,   10,  7, 0, "B" },
  { SOP_TEXT,   16,  7, 0, "C" },
  { SOP_TEXT,    1,  1, 2, "PICK YOUR EGG" },
  { SOP_SPRITE,  3,  3, 5, 0 },   /* egg_btc */
  { SOP_SPRITE, 13,  3, 5, 0 },   /* egg_btc */
  { SOP_TEXT,    1,  6, 0, "SWIFT" },
  { SOP_TEXT,   11,  6, 0, "ANCIENT" },
  { SOP_TEXT,    1,  8, 0, "> one key, WIF" },
  { SOP_TEXT,    1,  9, 0, "  12 words BIP39" },
  { SOP_TEXT,    1, 11, 0, "Hatches in about" },
  { SOP_TEXT,    1, 12, 0, "70 seconds. Back" },
  { SOP_TEXT,    1, 13, 0, "up the WIF key." },
  { SOP_TEXT,    1, 16, 0, "A:pick  <>:switch" },
  { SOP_TEXT,    1,  1, 2, "PICK YOUR EGG" },
  { SOP_SPRITE,  3,  3, 5, 0 },   /* egg_btc */
  { SOP_SPRITE, 13,  3, 5, 0 },   /* egg_btc */
  { SOP_TEXT,    1,  6, 0, "SWIFT" },
  { SOP_TEXT,   11,  6, 0, "ANCIENT" },
  { SOP_TEXT,    1,  8, 0, "  one key, WIF" },
  { SOP_TEXT,    1,  9, 0, "> 12 words BIP39" },
  { SOP_TEXT,    1, 11, 0, "Needs ~20 minutes" },
  { SOP_TEXT,    1, 12, 0, "of patience. Any" },
  { SOP_TEXT,    1, 13, 0, "wallet restores it." },
  { SOP_TEXT,    1, 16, 0, "A:pick  <>:switch" },
  { SOP_TEXT,    1,  0, 2, "FLIP YOUR OWN KEY" },
  { SOP_TEXT,    1,  1, 0, "Each press is one" },
  { SOP_TEXT,    1,  2, 0, "coin flip. H=1 T=0" },
  { SOP_TEXT,    3,  1, 2, "THE EGG IS" },
  { SOP_TEXT,    3,  2, 2, "HATCHING!" },
  { SOP_SPRITE,  9,  4, 5, 0 },   /* egg_btc */
  { SOP_TEXT,    1,  7, 0, "Do not turn off" },
  { SOP_TEXT,    1,  8, 0, "the power!" },
  { SOP_TEXT,    1, 14, 0, "deriving key..." },
  { SOP_TEXT,    1, 14, 0, "stretching seed" },
  { SOP_TEXT,    1, 15, 0, "2048 rounds..." },
  { SOP_BLANK,   1, 15, 18, 0 },
  { SOP_TEXT,    1, 14, 0, "deriving path..." },
  { SOP_TEXT,    1,  0, 2, "OFFWALLET" },
  { SOP_SPRITE, 16,  0, 1, 0 },   /* ball */
  { SOP_TEXT,    2,  6, 0, "ADDRESS" },
  { SOP_TEXT,    2,  8, 0, "RECEIVE (QR)" },
  { SOP_TEXT,    2, 10, 0, "MY UTXOS" },
  { SOP_TEXT,    2, 12, 0, "SECRET" },
  { SOP_TEXT,    2, 14, 0, "OUTPUT KEY" },
  { SOP_TEXT,    2, 16, 0, "SAVE TO CART" },
  { SOP_TEXT,    1, 17, 0, "A: pick" },
  { SOP_TEXT,    1,  0, 2, "SCAN TO PAY ME" },
  { SOP_TEXT,    1, 17, 0, "B: back" },
  { SOP_TEXT,    1,  0, 2, "RECEIVE" },
  { SOP_TEXT,    1,  8, 0, "building the QR" },
  { SOP_TEXT,    1,  9, 0, "code..." },
  { SOP_TEXT,    1,  0, 2, "MY UTXOS" },
  { SOP_TEXT,    1, 13, 0, "A:add  SEL:delete" },
  { SOP_TEXT,    1, 14, 0, "B: back" },
  { SOP_TEXT,    1,  0, 2, "TXID (64 HEX)" },
  { SOP_TEXT,    1,  1, 0, "as the explorer" },
  { SOP_TEXT,    1, 16, 0, "A:add B:del SEL:esc" },
  { SOP_TEXT,    1, 17, 0, "START when full" },
  { SOP_TEXT,    1,  0, 2, "OUTPUT INDEX" },
  { SOP_TEXT,    1,  1, 0, "3 digits, e.g. 000" },
  { SOP_TEXT,    1, 16, 0, "A:add B:del SEL:esc" },
  { SOP_TEXT,    1, 17, 0, "START when full" },
  { SOP_TEXT,    1,  0, 2, "AMOUNT IN SATS" },
  { SOP_TEXT,    1,  1, 0, "10 digits, padded" },
  { SOP_TEXT,    1, 16, 0, "A:add B:del SEL:esc" },
  { SOP_TEXT,    1, 17, 0, "START when full" },
  { SOP_TEXT,    1,  0, 2, "YOUR ADDRESS" },
  { SOP_SPRITE, 16,  0, 3, 0 },   /* btc */
  { SOP_TEXT,    1,  8, 0, "P2TR / taproot" },
  { SOP_TEXT,    1, 11, 0, "Back up the key" },
  { SOP_TEXT,    1, 12, 0, "before using it." },
  { SOP_TEXT,    1, 16, 0, "B: back" },
  { SOP_TEXT,    1,  0, 1, "SECRET - KEEP SAFE" },
  { SOP_TEXT,    1,  2, 0, "WIF - import as" },
  { SOP_TEXT,    1,  3, 0, "a taproot key:" },
  { SOP_TEXT,    1,  9, 0, "hex:" },
  { SOP_TEXT,    1, 15, 0, "B: back" },
  { SOP_TEXT,    1,  0, 1, "SECRET - KEEP SAFE" },
  { SOP_TEXT,    1,  1, 0, "12 WORDS (BIP39)" },
  { SOP_TEXT,    1, 15, 0, "B: back" },
  { SOP_TEXT,    1,  0, 2, "OUTPUT KEY (x-only)" },
  { SOP_TEXT,    1,  7, 0, "scriptPubKey:" },
  { SOP_TEXT,    1,  8, 0, "OP_1 <32-byte>" },
  { SOP_TEXT,    1, 10, 0, "Compare this with" },
  { SOP_TEXT,    1, 11, 0, "the wallet you" },
  { SOP_TEXT,    1, 12, 0, "import the key in." },
  { SOP_TEXT,    1, 16, 0, "B: back" },
};

/* Sprite ops need a tile base that is only known at compile time, so
   they are kept in a parallel table rather than inside story_op. */
/* size of each sprite op, tiles wide in the high nibble. */
const uint8_t STORY_OP_SIZE[99] = {
  0x22, 0, 0, 0, 0, 0, 0, 0, 0x22, 0x22, 0x22, 0, 0, 0, 0, 0x22, 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x22, 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t STORY_OP_TILE[99] = {
  (uint8_t)(STORY_TILE_BASE + 16), 0, 0, 0, 0, 0, 0, 0, (uint8_t)(STORY_TILE_BASE + 4), (uint8_t)(STORY_TILE_BASE + 8), (uint8_t)(STORY_TILE_BASE + 12), 0, 0, 0, 0, (uint8_t)(STORY_TILE_BASE + 4), (uint8_t)(STORY_TILE_BASE + 4), 0, 0, 0, 0, 0, 0, 0, 0, 0, (uint8_t)(STORY_TILE_BASE + 4), (uint8_t)(STORY_TILE_BASE + 4), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (uint8_t)(STORY_TILE_BASE + 4), 0, 0, 0, 0, 0, 0, 0, 0, (uint8_t)(STORY_TILE_BASE + 0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (uint8_t)(STORY_TILE_BASE + 16), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const story_screen STORY_SCREENS[20] = {
  {   0,  7, 1 },   /* title */
  {   7,  7, 1 },   /* starter */
  {  14, 11, 1 },   /* egg_swift */
  {  25, 11, 1 },   /* egg_ancient */
  {  36,  3, 1 },   /* flip_coin */
  {  39,  5, 1 },   /* hatch */
  {  44,  1, 0 },   /* hatch_key */
  {  45,  2, 0 },   /* hatch_stretch */
  {  47,  2, 0 },   /* hatch_path */
  {  49,  9, 1 },   /* wallet */
  {  58,  2, 1 },   /* receive */
  {  60,  3, 1 },   /* receive_wait */
  {  63,  3, 1 },   /* utxo_list */
  {  66,  4, 1 },   /* utxo_txid */
  {  70,  4, 1 },   /* utxo_vout */
  {  74,  4, 1 },   /* utxo_value */
  {  78,  6, 1 },   /* address */
  {  84,  5, 1 },   /* secret_wif */
  {  89,  3, 1 },   /* secret_words */
  {  92,  7, 1 },   /* output_key */
};

const story_box STORY_BOXES[15] = {
  {   0, "Hello! Welcome", "to the world of", "BLOCKCHAINS!" },
  {   0, "They call me the", "BLOCKCHAIN PROF.", "Others say SATOSHI" },
  {   0, "This world is", "home to creatures", "called UTXOs!" },
  {   0, "Some keep them on", "exchanges. That is", "not keeping them." },
  {   0, "Your own key story", "is about to begin.", "Let us go!" },
  { 255, "BITCOIN!", "The only starter", "that is real." },
  { 255, "That one never", "hatched in this", "world. Take BTC." },
  {   0, "You chose BITCOIN!", "A fine choice.", "The only one." },
  { 255, "128 flips. That", "is your key, and", "you watched it" },
  { 255, "land. The egg is", "warm. Something", "moves inside..." },
  {   0, "Congratulations!", "Your UTXO home is", "ready!" },
  { 255, "The egg did not", "hatch. Reset and", "try again." },
  { 255, "Saved to the", "cart battery.", "Whoever holds it" },
  { 255, "can read the key,", "so treat the cart", "like cash." },
  {   0, "Welcome back!", "Your wallet is in", "the cart." },
};

const story_dialog STORY_DIALOGS[9] = {
  {   0, 5 },   /* intro */
  {   5, 1 },   /* starter_ok */
  {   6, 1 },   /* starter_locked */
  {   7, 1 },   /* starter_chosen */
  {   8, 2 },   /* flips_done */
  {  10, 1 },   /* congrats */
  {  11, 1 },   /* hatch_failed */
  {  12, 2 },   /* saved */
  {  14, 1 },   /* welcome_back */
};

/* Extra CGB background palettes defined in the content file. */
const uint16_t STORY_PALETTES[32] = {
  RGB(31,31,27), RGB(22,22,18), RGB(10,10,12), RGB(2,2,4),
  RGB(31,31,31), RGB(31,10,10), RGB(6,4,6), RGB(20,4,4),
  RGB(31,31,31), RGB(31,22,6), RGB(8,5,2), RGB(26,15,3),
  RGB(31,31,31), RGB(26,14,2), RGB(8,4,1), RGB(8,4,1),
  RGB(31,31,31), RGB(30,30,27), RGB(7,8,24), RGB(9,9,10),
  RGB(30,30,27), RGB(31,21,3), RGB(18,10,0), RGB(18,10,0),
  RGB(31,31,31), RGB(30,30,27), RGB(22,14,10), RGB(9,9,10),
  RGB(30,30,27), RGB(29,23,17), RGB(15,15,15), RGB(6,3,2),
};
const uint8_t STORY_PALETTE_FIRST = 0;
const uint8_t STORY_PALETTE_COUNT = 8;
