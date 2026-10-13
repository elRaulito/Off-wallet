#pragma bank 1
/* Scene flow: title -> the professor -> pick the starter blockchain -> choose
   an egg -> flip 128 coins (entropy) -> hatch (key derivation) -> wallet.

   No text lives here. Every screen, line of dialogue and character comes from
   content/story.txt, compiled by tools/gen/gen_story.py; this file only holds
   the flow and the parts that depend on runtime data (the address, the key,
   the coin flips, menu cursors). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>

#include "game.h"
#include "ui.h"
#include "rng.h"
#include "story.h"
#include "gfx/gfx_tiles.h"
#include "crypto/secp256k1.h"
#include "crypto/wallet.h"
#include "crypto/bip39.h"
#include "crypto/base58.h"
#include "crypto/sha256.h"
#include "crypto/hmac512.h"
#include "progress.h"
#include "qr.h"
#include "qr_draw.h"
#include "utxo.h"

/* ---- saved wallet, kept in cartridge SRAM ---- */
#define SAVE_MAGIC 0x4F57UL      /* "OW" */

typedef struct {
    uint16_t magic;
    uint8_t  kind;               /* 0 = single key, 1 = BIP-39 seed phrase */
    uint16_t words[12];
    uint8_t  privkey[32];
    uint8_t  output_key[32];
    char     address[64];
    uint8_t  checksum[4];
} save_t;

static save_t __at(0xA000) sram;

/* ---- work area (WRAM) ----
   wa/words/kind are deliberately non-static so tools/run_game_test.py can read
   the generated wallet straight out of an emulator's memory and check it. */
wallet_addr     wa;
uint16_t        words[12];
uint8_t         kind;
static uint8_t  seed[64];
static uint8_t  entropy[RNG_BYTES];
static char     line[64];
static char     mnemonic[128];

/* The animation handler and the crypto progress callback live in bank 0
   (progress.c): they are called through function pointers while another bank
   is mapped, so they cannot sit in this bank. */

/* ---------------- helpers ---------------- */

static void hex4(char *dst, const uint8_t *b, uint8_t n)
{
    static const char H[17] = "0123456789abcdef";
    uint8_t i;
    for (i = 0; i < n; i++) {
        dst[i * 2]     = H[b[i] >> 4];
        dst[i * 2 + 1] = H[b[i] & 15];
    }
    dst[n * 2] = 0;
}

static void substr(char *dst, const char *src, uint8_t from, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len && src[from + i]; i++) dst[i] = src[from + i];
    dst[i] = 0;
}

/* Wait for a keypress, releasing first. Without the release wait, a held
   button is read again by the next scene and whole screens flash past in a
   single frame. Spins the entropy counter while idle. */
static uint8_t wait_key(void)
{
    uint8_t k;
    while (joypad()) { rng_spin(); vsync(); }
    for (;;) {
        k = joypad();
        if (k) return k;
        rng_spin();
        vsync();
    }
}

/* ---------------- scenes ---------------- */

static void scene_title(void) BANKED
{
    story_screen_draw(SCR_TITLE);
    for (;;) {
        if (wait_key() & (J_START | J_A)) break;
    }
}

static void scene_prof(void) BANKED
{
    ui_clear();
    story_dialog_run(DLG_INTRO);
}

/* Three balls on the table; only the unlocked one can be opened. */
static void scene_starter(void) BANKED
{
    static const uint8_t cur_x[3] = { CFG_STARTER_CUR0_X, CFG_STARTER_CUR1_X,
                                      CFG_STARTER_CUR2_X };
    static const uint8_t cur_y[3] = { CFG_STARTER_CUR0_Y, CFG_STARTER_CUR1_Y,
                                      CFG_STARTER_CUR2_Y };
    uint8_t sel = CFG_STARTER_UNLOCKED;
    uint8_t i, k;

    story_screen_draw(SCR_STARTER);
    for (;;) {
        for (i = 0; i < 3; i++)
            ui_text(cur_x[i], cur_y[i], (i == sel) ? "^" : " ");

        story_dialog_run(sel == CFG_STARTER_UNLOCKED ? DLG_STARTER_OK
                                                     : DLG_STARTER_LOCKED);

        k = wait_key();
        if (k & J_RIGHT) { if (sel < 2) sel++; }
        else if (k & J_LEFT) { if (sel) sel--; }
        else if ((k & (J_A | J_START)) && sel == CFG_STARTER_UNLOCKED) break;
    }
    ui_clear();
    story_dialog_run(DLG_STARTER_CHOSEN);
}

/* Which kind of wallet: single key (fast) or 12-word BIP-39 (slow).
   The two screens in the content file each draw the whole chooser, so the
   highlighted entry is whatever that screen says it is. */
static uint8_t scene_egg_kind(void) BANKED
{
    uint8_t sel = 0, k;
    for (;;) {
        story_screen_draw(sel ? SCR_EGG_ANCIENT : SCR_EGG_SWIFT);
        k = wait_key();
        if (k & (J_LEFT | J_RIGHT)) sel ^= 1;
        else if (k & (J_A | J_START)) return sel;
    }
}

/* ---------------- entropy: 128 coin flips ----------------
   Each press lands one flip, which goes straight onto the grid. The tally
   underneath is there to be checked: 128 fair flips land near 64/64, and a
   run much past 8 of a kind is a reason to flip the lot again. The bits on
   that screen are the key -- BIP-39 entropy on the 12-word path, SHA-256 of
   them on the single-key path -- so writing them down reproduces it. */

static void num3(char *dst, uint8_t v)
{
    dst[0] = (char)('0' + v / 100);
    dst[1] = (char)('0' + (v / 10) % 10);
    dst[2] = (char)('0' + v % 10);
}

static void flip_cell(uint8_t i, uint8_t pal)
{
    uint8_t x = (uint8_t)(CFG_FLIP_X + i % CFG_FLIP_COLS);
    uint8_t y = (uint8_t)(CFG_FLIP_Y + i / CFG_FLIP_COLS);
    line[0] = rng_bit(i) ? 'H' : 'T';
    line[1] = 0;
    ui_text(x, y, line);
    ui_attr(x, y, 1, pal);
}

/* "flips 128 of 128" and "H:064 T:064 R:05" */
static void flip_stats(void)
{
    uint8_t i, b, heads = 0, run = 0, best = 0;
    uint8_t n = rng_bits();

    for (i = 0; i < n; i++) {
        b = rng_bit(i);
        if (b) heads++;
        if (i && b == rng_bit((uint8_t)(i - 1))) run++;
        else run = 1;
        if (run > best) best = run;
    }

    line[0] = 'f'; line[1] = 'l'; line[2] = 'i'; line[3] = 'p';
    line[4] = 's'; line[5] = ' ';
    num3(line + 6, n);
    line[9] = ' '; line[10] = 'o'; line[11] = 'f'; line[12] = ' ';
    num3(line + 13, RNG_TARGET_BITS);
    line[16] = 0;
    ui_text(1, CFG_FLIP_COUNT_Y, line);

    line[0] = 'H'; line[1] = ':';
    num3(line + 2, heads);
    line[5] = ' '; line[6] = 'T'; line[7] = ':';
    num3(line + 8, (uint8_t)(n - heads));
    line[11] = ' '; line[12] = 'R'; line[13] = ':';
    line[14] = (char)('0' + best / 10);
    line[15] = (char)('0' + best % 10);
    line[16] = 0;
    ui_text(1, CFG_FLIP_STATS_Y, line);
}

static void scene_flip_coins(void) BANKED
{
    uint8_t i, k;

    for (;;) {
        rng_clear();
        story_screen_draw(SCR_FLIP_COIN);
        flip_stats();

        while (!rng_ready()) {
            i = rng_bits();
            rng_flip();
            vsync();                 /* draw in the blank, not mid-frame */
            if (i) flip_cell((uint8_t)(i - 1), PAL_TEXT);
            flip_cell(i, PAL_ACCENT);
            flip_stats();
        }

        ui_text(1, CFG_FLIP_PROMPT_Y, "START: keep these");
        ui_text(1, (uint8_t)(CFG_FLIP_PROMPT_Y + 1), "SELECT: flip again");
        k = wait_key();
        if (!(k & J_SELECT)) break;
    }
    story_dialog_run(DLG_FLIPS_DONE);
}

/* Derivation, with the shiver animation running off the VBlank handler. */
static uint8_t scene_hatch(void) BANKED
{
    uint8_t ok;

    story_screen_draw(SCR_HATCH);
    /* SWIFT does 2 scalar multiplications = 64 bumps.
       ANCIENT does 2047 PBKDF2 rounds + 4 scalar multiplications = 2175,
       divided by 9 to fit the 8-bit bar. */
    if (kind == 0) ow_progress_begin(64, 1);
    else           ow_progress_begin(241, 9);
    secp_progress_cb = ow_bump;
    pbkdf2_progress_cb = ow_bump;

    rng_extract(entropy, RNG_BYTES);   /* the 128 flips, as flipped */

    if (kind == 0) {
        /* Single key: entropy -> 32-byte private key -> P2TR output key. */
        uint8_t k32[32];
        sha256(entropy, 16, k32);
        story_screen_draw(SCR_HATCH_KEY);
        ok = wallet_single_p2tr(&wa, k32);
        {
            uint8_t i;
            for (i = 0; i < 12; i++) words[i] = 0;
        }
    } else {
        /* 12 words -> BIP-39 seed -> m/86'/0'/0'/0/0 */
        bip39_from_entropy(words, entropy);
        bip39_mnemonic(mnemonic, words);
        story_screen_draw(SCR_HATCH_STRETCH);
        bip39_seed(seed, mnemonic, (uint8_t)strlen(mnemonic));
        story_screen_draw(SCR_HATCH_PATH);
        ok = wallet_derive_p2tr(&wa, seed, 64, 0, 0);
    }

    secp_progress_cb = 0;
    pbkdf2_progress_cb = 0;
    ow_progress_end();
    return ok;
}

static void save_wallet(void) BANKED
{
    uint8_t i;
    uint8_t h[32];
    ENABLE_RAM;
    SWITCH_RAM(0);
    sram.magic = SAVE_MAGIC;
    sram.kind = kind;
    for (i = 0; i < 12; i++) sram.words[i] = words[i];
    for (i = 0; i < 32; i++) sram.privkey[i] = wa.privkey[i];
    for (i = 0; i < 32; i++) sram.output_key[i] = wa.output_key[i];
    for (i = 0; i < 63; i++) sram.address[i] = wa.address[i];
    sram.address[63] = 0;
    sha256((const uint8_t *)&sram, (uint16_t)(sizeof(save_t) - 4), h);
    for (i = 0; i < 4; i++) sram.checksum[i] = h[i];
    DISABLE_RAM;
}

static uint8_t load_wallet(void) BANKED
{
    uint8_t i;
    uint8_t h[32];
    ENABLE_RAM;
    SWITCH_RAM(0);
    if (sram.magic != SAVE_MAGIC) { DISABLE_RAM; return 0; }
    sha256((const uint8_t *)&sram, (uint16_t)(sizeof(save_t) - 4), h);
    for (i = 0; i < 4; i++) {
        if (sram.checksum[i] != h[i]) { DISABLE_RAM; return 0; }
    }
    kind = sram.kind;
    for (i = 0; i < 12; i++) words[i] = sram.words[i];
    for (i = 0; i < 32; i++) wa.privkey[i] = sram.privkey[i];
    for (i = 0; i < 32; i++) wa.output_key[i] = sram.output_key[i];
    for (i = 0; i < 64; i++) wa.address[i] = sram.address[i];
    DISABLE_RAM;
    return 1;
}

/* ---- wallet screens: static labels come from the content file, the values
       are filled in here ---- */

static void show_address(void) BANKED
{
    story_screen_draw(SCR_ADDRESS);
    /* 62 characters over four lines of 16 */
    substr(line, wa.address, 0, 16);  ui_text(2, 3, line);
    substr(line, wa.address, 16, 16); ui_text(2, 4, line);
    substr(line, wa.address, 32, 16); ui_text(2, 5, line);
    substr(line, wa.address, 48, 16); ui_text(2, 6, line);
    ui_text(1, 9, kind ? "m/86'/0'/0'/0/0" : "single key tr()");
    ui_wait_press();
}

static void show_secret(void) BANKED
{
    if (kind == 0) {
        story_screen_draw(SCR_SECRET_WIF);
        wif_encode(line, wa.privkey);
        substr(mnemonic, line, 0, 18);  ui_text(1, 5, mnemonic);
        substr(mnemonic, line, 18, 18); ui_text(1, 6, mnemonic);
        substr(mnemonic, line, 36, 18); ui_text(1, 7, mnemonic);
        hex4(line, wa.privkey, 8);       ui_text(1, 10, line);
        hex4(line, wa.privkey + 8, 8);   ui_text(1, 11, line);
        hex4(line, wa.privkey + 16, 8);  ui_text(1, 12, line);
        hex4(line, wa.privkey + 24, 8);  ui_text(1, 13, line);
    } else {
        /* One word per row: two columns cannot hold a two-digit number plus an
           eight-letter word inside 20 tiles, and this is easier to copy out. */
        uint8_t i, j, n;
        char w[10];
        story_screen_draw(SCR_SECRET_WORDS);
        for (i = 0; i < 12; i++) {
            n = 0;
            if (i + 1 >= 10) {
                line[n++] = '1';
                line[n++] = (char)('0' + (i + 1) % 10);
            } else {
                line[n++] = ' ';
                line[n++] = (char)('0' + i + 1);
            }
            line[n++] = '.';
            line[n++] = ' ';
            bip39_word(w, words[i]);
            for (j = 0; w[j]; j++) line[n++] = w[j];
            line[n] = 0;
            ui_text(3, (uint8_t)(2 + i), line);
        }
    }
    ui_wait_press();
}

/* The address as a QR code. bech32m is case-insensitive and its uppercase
   form fits QR's alphanumeric mode, which is what keeps the code small enough
   to draw on a 160x144 screen. */
static void show_receive(void) BANKED
{
    uint8_t i;
    char c;

    story_screen_draw(SCR_RECEIVE_WAIT);
    for (i = 0; wa.address[i] && i < 63; i++) {
        c = wa.address[i];
        line[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    line[i] = 0;

    if (!qr_encode(line)) {
        ui_text(1, 11, "QR failed");
        ui_wait_press();
        return;
    }
    story_screen_draw(SCR_RECEIVE);
    qr_draw(3, 2);
    ui_wait_press();
}

static void show_verify(void) BANKED
{
    story_screen_draw(SCR_OUTPUT_KEY);
    hex4(line, wa.output_key, 8);      ui_text(1, 2, line);
    hex4(line, wa.output_key + 8, 8);  ui_text(1, 3, line);
    hex4(line, wa.output_key + 16, 8); ui_text(1, 4, line);
    hex4(line, wa.output_key + 24, 8); ui_text(1, 5, line);
    ui_wait_press();
}

static void scene_wallet(void) BANKED
{
    uint8_t sel = 0, k;
    for (;;) {
        story_screen_draw(SCR_WALLET);
        substr(line, wa.address, 0, 18);
        ui_text(1, 2, line);
        substr(line, wa.address, 18, 18);
        ui_text(1, 3, line);
        {   /* balance from the UTXO list, if any has been entered */
            char b[24];
            utxo_load();
            if (utxo_count) {
                utxo_format_btc(b, utxo_total());
                ui_text(1, 4, b);
                ui_text(13, 4, "BTC");
            }
        }
        ui_text(CFG_MENU_X, (uint8_t)(CFG_MENU_Y + sel * CFG_MENU_STEP), ">");

        k = wait_key();
        if (k & J_DOWN)      { if (sel + 1 < CFG_MENU_COUNT) sel++; }
        else if (k & J_UP)   { if (sel) sel--; }
        else if (k & (J_A | J_START)) {
            if (sel == 0) show_address();
            else if (sel == 1) show_receive();
            else if (sel == 2) utxo_screen();
            else if (sel == 3) show_secret();
            else if (sel == 4) show_verify();
            else {
                save_wallet();
                story_dialog_run(DLG_SAVED);
            }
        }
    }
}

void game_run(void) BANKED
{
    add_VBL(ow_vbl);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();

    ui_init();
    rng_init();

    if (load_wallet()) {
        ui_clear();
        story_dialog_run(DLG_WELCOME_BACK);
        scene_wallet();
        return;
    }

    scene_title();
    scene_prof();
    scene_starter();
    kind = scene_egg_kind();
    scene_flip_coins();
    if (!scene_hatch()) {
        ui_clear();
        story_dialog_run(DLG_HATCH_FAILED);
        for (;;) vsync();
    }
    story_dialog_run(DLG_CONGRATS);
    save_wallet();
    scene_wallet();
}
