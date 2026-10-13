#pragma bank 1
/* The UTXO list: what the wallet believes it owns.
 *
 * A Game Boy has no network, so the entries are typed in by hand from a block
 * explorer: transaction id, output index and amount. They live in their own
 * region of cartridge SRAM, separate from the wallet record, so an old save
 * stays readable.
 *
 * This is the data a spend needs; it is also what makes a balance possible.
 */
#include <gb/gb.h>
#include "utxo.h"
#include "ui.h"
#include "story.h"
#include "crypto/sha256.h"

#define UTXO_MAGIC 0x5558U               /* "UX" */

typedef struct {
    uint16_t magic;
    utxo_t   u[UTXO_MAX];
    uint8_t  checksum[4];
} utxo_store;

/* 0xA000 holds the wallet record; the UTXO table starts well clear of it. */
static utxo_store __at(0xA100) usram;

utxo_t   utxo_tab[UTXO_MAX];
uint8_t  utxo_count;

static void store_checksum(uint8_t out[4]) BANKED
{
    uint8_t h[32];
    uint8_t i;
    sha256((const uint8_t *)&usram, (uint16_t)(sizeof(utxo_store) - 4), h);
    for (i = 0; i < 4; i++) out[i] = h[i];
}

void utxo_load(void) BANKED
{
    uint8_t i;
    uint8_t chk[4];

    for (i = 0; i < UTXO_MAX; i++) utxo_tab[i].used = 0;
    utxo_count = 0;

    ENABLE_RAM;
    SWITCH_RAM(0);
    if (usram.magic != UTXO_MAGIC) { DISABLE_RAM; return; }
    store_checksum(chk);
    for (i = 0; i < 4; i++) {
        if (usram.checksum[i] != chk[i]) { DISABLE_RAM; return; }
    }
    for (i = 0; i < UTXO_MAX; i++) {
        utxo_tab[i] = usram.u[i];
        if (utxo_tab[i].used) utxo_count++;
    }
    DISABLE_RAM;
}

void utxo_save(void) BANKED
{
    uint8_t i;
    ENABLE_RAM;
    SWITCH_RAM(0);
    usram.magic = UTXO_MAGIC;
    for (i = 0; i < UTXO_MAX; i++) usram.u[i] = utxo_tab[i];
    store_checksum(usram.checksum);
    DISABLE_RAM;
}

/* Total of the used entries, in satoshis. 32 bits caps a slot at 42.9 BTC,
   which is far past what belongs on a Game Boy. */
uint32_t utxo_total(void) BANKED
{
    uint32_t t = 0;
    uint8_t i;
    for (i = 0; i < UTXO_MAX; i++)
        if (utxo_tab[i].used) t += utxo_tab[i].value;
    return t;
}

/* ------------------------------------------------------------ formatting */

/* value in satoshis -> "0.00012345" (8 decimals, trailing zeros kept) */
void utxo_format_btc(char *dst, uint32_t sats) BANKED
{
    uint32_t whole = sats / 100000000UL;
    uint32_t frac  = sats % 100000000UL;
    uint8_t n = 0, i;
    char digits[9];

    if (whole >= 10) dst[n++] = (char)('0' + (uint8_t)(whole / 10));
    dst[n++] = (char)('0' + (uint8_t)(whole % 10));
    dst[n++] = '.';
    for (i = 8; i > 0; i--) {
        digits[i - 1] = (char)('0' + (uint8_t)(frac % 10));
        frac /= 10;
    }
    for (i = 0; i < 8; i++) dst[n++] = digits[i];
    dst[n] = 0;
}

void utxo_format_sats(char *dst, uint32_t sats) BANKED
{
    char tmp[11];
    uint8_t n = 0, i;
    if (!sats) { dst[0] = '0'; dst[1] = 0; return; }
    while (sats) {
        tmp[n++] = (char)('0' + (uint8_t)(sats % 10));
        sats /= 10;
    }
    for (i = 0; i < n; i++) dst[i] = tmp[n - 1 - i];
    dst[n] = 0;
}

static void hexbyte(char *dst, uint8_t v) BANKED
{
    static const char H[17] = "0123456789abcdef";
    dst[0] = H[v >> 4];
    dst[1] = H[v & 15];
}

/* --------------------------------------------------- entry widgets ------
   There is no keyboard and no link cable, so values are picked one character
   at a time from a row of digits: left/right to move the picker, A to append,
   B to rub out, START to accept. Tedious for a 64-character transaction id,
   but it is what the hardware offers. */

static uint8_t wait_key_u(void) BANKED
{
    uint8_t k;
    while (joypad()) vsync();
    for (;;) {
        k = joypad();
        if (k) return k;
        vsync();
    }
}

static const char HEXDIG[17] = "0123456789abcdef";

/* Collect `nchars` characters from `set` into dst. Returns 0 if cancelled. */
static uint8_t pick_entry(char *dst, uint8_t nchars, const char *set,
                          uint8_t setlen, uint8_t row0, uint8_t percol) BANKED
{
    uint8_t n = 0, sel = 0, i, k;
    char buf[2];

    for (;;) {
        /* what has been typed so far */
        for (i = 0; i < 4; i++) ui_blank(1, (uint8_t)(row0 + i), 18);
        for (i = 0; i < n; i++) {
            buf[0] = dst[i];
            buf[1] = 0;
            ui_text((uint8_t)(1 + (i % percol)),
                    (uint8_t)(row0 + i / percol), buf);
        }
        /* the picker */
        ui_blank(1, 13, 18);
        for (i = 0; i < setlen; i++) {
            buf[0] = set[i];
            buf[1] = 0;
            ui_text((uint8_t)(1 + i), 13, buf);
        }
        ui_blank(1, 14, 18);
        ui_text((uint8_t)(1 + sel), 14, "^");

        k = wait_key_u();
        if (k & J_RIGHT) { if (sel + 1 < setlen) sel++; else sel = 0; }
        else if (k & J_LEFT) { if (sel) sel--; else sel = (uint8_t)(setlen - 1); }
        else if (k & J_A) { if (n < nchars) dst[n++] = set[sel]; }
        else if (k & J_B) { if (n) n--; }
        else if (k & J_SELECT) return 0;
        else if (k & J_START) {
            if (n == nchars) { dst[n] = 0; return 1; }
        }
    }
}

/* 64 hex characters -> 32 bytes, most significant first (explorer order). */
static uint8_t hex_entry(uint8_t *out32) BANKED
{
    char hx[70];
    uint8_t i;
    if (!pick_entry(hx, 64, HEXDIG, 16, 3, 18)) return 0;
    for (i = 0; i < 32; i++) {
        uint8_t hi = 0, lo = 0, j;
        for (j = 0; j < 16; j++) {
            if (hx[i * 2] == HEXDIG[j]) hi = j;
            if (hx[i * 2 + 1] == HEXDIG[j]) lo = j;
        }
        out32[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

static uint8_t num_entry(uint32_t *out, uint8_t maxdigits) BANKED
{
    char ds[12];
    uint8_t i;
    uint32_t v = 0;
    if (!pick_entry(ds, maxdigits, HEXDIG, 10, 3, 18)) return 0;
    for (i = 0; ds[i]; i++) v = v * 10UL + (uint32_t)(ds[i] - '0');
    *out = v;
    return 1;
}

/* --------------------------------------------------------- the screens */

static void draw_list(uint8_t sel) BANKED
{
    char buf[24];
    uint8_t i;

    story_screen_draw(SCR_UTXO_LIST);
    utxo_format_btc(buf, utxo_total());
    ui_text(1, 2, buf);
    ui_text(13, 2, "BTC");

    for (i = 0; i < UTXO_MAX; i++) {
        uint8_t y = (uint8_t)(4 + i);
        ui_text(0, y, (i == sel) ? ">" : " ");
        if (!utxo_tab[i].used) {
            ui_text(2, y, "- empty -");
            continue;
        }
        /* first four bytes of the txid, the output index and the amount */
        hexbyte(buf + 0, utxo_tab[i].txid[0]);
        hexbyte(buf + 2, utxo_tab[i].txid[1]);
        hexbyte(buf + 4, utxo_tab[i].txid[2]);
        buf[6] = ':';
        buf[7] = (char)('0' + (utxo_tab[i].vout % 10));
        buf[8] = ' ';
        utxo_format_sats(buf + 9, utxo_tab[i].value);
        ui_text(2, y, buf);
    }
}

void utxo_screen(void) BANKED
{
    uint8_t sel = 0, k, i;

    utxo_load();
    for (;;) {
        draw_list(sel);
        k = wait_key_u();
        if (k & J_DOWN) { if (sel + 1 < UTXO_MAX) sel++; }
        else if (k & J_UP) { if (sel) sel--; }
        else if (k & J_B) return;
        else if (k & J_SELECT) {
            if (utxo_tab[sel].used) {
                utxo_tab[sel].used = 0;
                utxo_count--;
                utxo_save();
            }
        }
        else if (k & (J_A | J_START)) {
            utxo_t e;
            uint32_t n32;
            for (i = 0; i < 32; i++) e.txid[i] = 0;

            story_screen_draw(SCR_UTXO_TXID);
            if (!hex_entry(e.txid)) continue;
            story_screen_draw(SCR_UTXO_VOUT);
            if (!num_entry(&n32, 3)) continue;
            e.vout = (uint8_t)n32;
            story_screen_draw(SCR_UTXO_VALUE);
            if (!num_entry(&n32, 10)) continue;
            e.value = n32;
            e.used = 1;
            utxo_tab[sel] = e;
            utxo_count = 0;
            for (i = 0; i < UTXO_MAX; i++) if (utxo_tab[i].used) utxo_count++;
            utxo_save();
        }
    }
}
