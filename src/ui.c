#pragma bank 1
/* Text and dialogue rendering. The font is loaded so that tile id == ASCII
   code - 0x20, which lets strings be pushed straight into the tile map with
   set_bkg_tiles: no printf, no console layer. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "ui.h"
#include "gfx/font_tiles.h"
#include "gfx/gfx_tiles.h"
#include "progress.h"
#include "story.h"

#define SCR_W 20
#define SCR_H 18

static uint8_t linebuf[SCR_W + 1];

/* Every palette, text included, is declared in content/story.txt and lands
   in STORY_PALETTES; see tools/gen/gen_story.py. */

void ui_init(void) BANKED
{
    ow_load_gfx();                 /* must run from bank 0: see progress.c */
    set_bkg_palette(0, STORY_PALETTE_COUNT, STORY_PALETTES);
    ui_clear();
    SHOW_BKG;
    DISPLAY_ON;
}

void ui_clear(void) BANKED
{
    uint8_t x, y;
    for (x = 0; x < SCR_W; x++) linebuf[x] = FONT_TILE(' ');
    for (y = 0; y < SCR_H; y++) set_bkg_tiles(0, y, SCR_W, 1, linebuf);
    /* reset every tile to palette 0 */
    VBK_REG = 1;
    for (x = 0; x < SCR_W; x++) linebuf[x] = 0;
    for (y = 0; y < SCR_H; y++) set_bkg_tiles(0, y, SCR_W, 1, linebuf);
    VBK_REG = 0;
}

/* Draw text. Characters outside the font fall back to space. */
void ui_text(uint8_t x, uint8_t y, const char *s) BANKED
{
    uint8_t n = 0;
    while (s[n] && (uint8_t)(x + n) < SCR_W) {
        uint8_t c = (uint8_t)s[n];
        linebuf[n] = (c >= FONT_FIRST_CHAR && c < FONT_FIRST_CHAR + FONT_NGLYPH)
                     ? (uint8_t)(c - FONT_FIRST_CHAR) : 0;
        n++;
    }
    if (n) set_bkg_tiles(x, y, n, 1, linebuf);
}

/* Set the CGB palette of a run of tiles. */
void ui_attr(uint8_t x, uint8_t y, uint8_t w, uint8_t pal) BANKED
{
    uint8_t i;
    for (i = 0; i < w && i < SCR_W; i++) linebuf[i] = pal;
    VBK_REG = 1;
    set_bkg_tiles(x, y, w, 1, linebuf);
    VBK_REG = 0;
}

/* A framed window. w and h count the whole box including the border. */
void ui_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) BANKED
{
    uint8_t i, j;
    linebuf[0] = GFX_BASE + T_BOX_TL;
    for (i = 1; i < w - 1; i++) linebuf[i] = GFX_BASE + T_BOX_T;
    linebuf[w - 1] = GFX_BASE + T_BOX_TR;
    set_bkg_tiles(x, y, w, 1, linebuf);

    linebuf[0] = GFX_BASE + T_BOX_L;
    for (i = 1; i < w - 1; i++) linebuf[i] = FONT_TILE(' ');
    linebuf[w - 1] = GFX_BASE + T_BOX_R;
    for (j = 1; j < h - 1; j++) set_bkg_tiles(x, y + j, w, 1, linebuf);

    linebuf[0] = GFX_BASE + T_BOX_BL;
    for (i = 1; i < w - 1; i++) linebuf[i] = GFX_BASE + T_BOX_B;
    linebuf[w - 1] = GFX_BASE + T_BOX_BR;
    set_bkg_tiles(x, y + h - 1, w, 1, linebuf);
}

/* 2x2 graphic drawn into the background, tile index relative to GFX_BASE. */
void ui_blit2x2(uint8_t x, uint8_t y, uint8_t first_tile, uint8_t pal) BANKED
{
    ui_blit2x2_raw(x, y, (uint8_t)(GFX_BASE + first_tile), pal);
}

void ui_blit2x2_raw(uint8_t x, uint8_t y, uint8_t first_tile, uint8_t pal) BANKED
{
    ui_blit(x, y, 2, 2, first_tile, pal);
}

void ui_blit(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
             uint8_t first_tile, uint8_t pal) BANKED
{
    uint8_t r, i;
    for (r = 0; r < h; r++) {
        for (i = 0; i < w; i++)
            linebuf[i] = (uint8_t)(first_tile + r * w + i);
        set_bkg_tiles(x, (uint8_t)(y + r), w, 1, linebuf);
        ui_attr(x, (uint8_t)(y + r), w, pal);
    }
}

/* Erase a run of tiles without touching the rest of the row. */
void ui_blank(uint8_t x, uint8_t y, uint8_t w) BANKED
{
    uint8_t i;
    if (x >= SCR_W) return;
    if (x + w > SCR_W) w = (uint8_t)(SCR_W - x);
    for (i = 0; i < w; i++) linebuf[i] = FONT_TILE(' ');
    set_bkg_tiles(x, y, w, 1, linebuf);
}

void ui_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t filled) BANKED
{
    uint8_t i;
    for (i = 0; i < w; i++)
        linebuf[i] = GFX_BASE + (i < filled ? T_BAR_FULL : T_BAR_EMPTY);
    set_bkg_tiles(x, y, w, 1, linebuf);
}

uint8_t ui_wait_press(void) BANKED
{
    uint8_t k;
    while (joypad()) vsync();          /* wait for release first */
    for (;;) {
        k = joypad();
        if (k) return k;
        vsync();
    }
}

/* Pokemon-style dialogue: up to three lines in a box at the bottom, revealed
   a character at a time, then "press a button". */
void ui_dialog(const char *l1, const char *l2, const char *l3) BANKED
{
    const char *lines[3];
    uint8_t li, n;
    lines[0] = l1; lines[1] = l2; lines[2] = l3;

    ui_box(0, 12, 20, 6);
    for (li = 0; li < 3; li++) {
        const char *s = lines[li];
        if (!s) continue;
        for (n = 1; s[n - 1]; n++) {
            char part[21];
            uint8_t i;
            for (i = 0; i < n && s[i]; i++) part[i] = s[i];
            part[i] = 0;
            ui_text(1, (uint8_t)(13 + li), part);
            vsync(); vsync();
            if (joypad()) {                 /* let the player skip the crawl */
                ui_text(1, (uint8_t)(13 + li), s);
                break;
            }
        }
    }
    ui_text(17, 17, "..");
    ui_wait_press();
}
