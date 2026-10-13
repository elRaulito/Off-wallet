/* Interrupt handler and crypto progress callback.

   These MUST live in bank 0. They are reached through function pointers
   (add_VBL, secp_progress_cb) at moments when the crypto code has some other
   bank mapped at 0x4000 -- a pointer carries no bank number, so a handler in a
   switchable bank would be jumped into as data. Bank 0 is always mapped.

   For the same reason the progress bar is drawn straight into VRAM here
   instead of calling the banked ui_* helpers. */
#include <gb/gb.h>
#include <gb/hardware.h>
#include "progress.h"
#include "ui.h"
#include "gfx/gfx_tiles.h"
#include "gfx/font_tiles.h"
#include "story.h"

volatile uint16_t ow_anim;
volatile uint8_t  ow_anim_on;
volatile uint8_t  ow_progress;
volatile uint8_t  ow_progress_max = 64;
static   uint8_t  bump_div = 1;
static   uint8_t  bump_acc;

#define BAR_X 2
#define BAR_Y 11
#define BAR_W 16
#define TILEMAP ((uint8_t *)0x9800)

static void draw_bar(void)
{
    uint8_t filled, i;
    uint16_t p = (uint16_t)ow_progress * BAR_W;
    filled = (uint8_t)(p / (ow_progress_max ? ow_progress_max : 1));
    if (filled > BAR_W) filled = BAR_W;
    /* safe: we are inside VBlank */
    for (i = 0; i < BAR_W; i++)
        TILEMAP[BAR_Y * 32 + BAR_X + i] =
            (uint8_t)(GFX_BASE + (i < filled ? T_BAR_FULL : T_BAR_EMPTY));
}

void ow_vbl(void)
{
    ow_anim++;
    if (!ow_anim_on) return;
    /* the egg shivers: nudge the whole background by a pixel now and then */
    SCX_REG = (uint8_t)(((ow_anim >> 4) & 1) ? 1 : 0);
    if ((ow_anim & 7) == 0) draw_bar();
}

/* Called from deep inside the scalar multiplications / PBKDF2 rounds. */
void ow_bump(void)
{
    if (++bump_acc < bump_div) return;
    bump_acc = 0;
    if (ow_progress < ow_progress_max) ow_progress++;
}

/* `divisor` lets a long job (PBKDF2 does 2047 rounds) drive an 8-bit bar:
   only every Nth bump advances it. */
void ow_progress_begin(uint8_t total_steps, uint8_t divisor)
{
    ow_progress = 0;
    ow_progress_max = total_steps ? total_steps : 1;
    bump_div = divisor ? divisor : 1;
    bump_acc = 0;
    ow_anim_on = 1;
}

/* Tiles live in bank 5. Mapping that bank has to happen from bank 0, and the
   caller's bank has to be restored before returning into it. */
void ow_load_gfx(void)
{
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(FONT_BANK);
    set_bkg_data(0, FONT_NGLYPH, FONT_TILES);
    set_bkg_data(GFX_BASE, GFX_NTILES, GFX_TILES);
#if STORY_NTILES > 0
    /* sprites drawn as ASCII art in content/story.txt, also in bank 5 */
    set_bkg_data(STORY_TILE_BASE, STORY_NSPRITES * 4, STORY_TILES);
#endif
    SWITCH_ROM(saved);
}

void ow_progress_end(void)
{
    ow_progress = ow_progress_max;
    ow_anim_on = 0;
    SCX_REG = 0;
}
