#ifndef OW_UI_H
#define OW_UI_H
#include <stdint.h>
#include <gb/gb.h>

/* Graphics tiles sit right after the 95 font glyphs in VRAM. */
#define GFX_BASE        95
#define FONT_TILE(c)    ((uint8_t)((c) - 0x20))

/* PAL_TEXT, PAL_WARN and PAL_ACCENT come from story_data.h: the palettes are
   declared in content/story.txt, so their slot numbers are generated. */
#include "story_data.h"

void    ui_init(void) BANKED;
void    ui_clear(void) BANKED;
void    ui_text(uint8_t x, uint8_t y, const char *s) BANKED;
void    ui_attr(uint8_t x, uint8_t y, uint8_t w, uint8_t pal) BANKED;
void    ui_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) BANKED;
void    ui_blit2x2(uint8_t x, uint8_t y, uint8_t first_tile, uint8_t pal) BANKED;
/* Same, but first_tile is an absolute VRAM tile index (story sprites live
   past the built-in graphics, so they cannot be GFX_BASE-relative). */
void    ui_blit2x2_raw(uint8_t x, uint8_t y, uint8_t first_tile, uint8_t pal) BANKED;
/* Any size: w by h tiles, taken in reading order from first_tile. */
void    ui_blit(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                uint8_t first_tile, uint8_t pal) BANKED;
void    ui_blank(uint8_t x, uint8_t y, uint8_t w) BANKED;
void    ui_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t filled) BANKED;
uint8_t ui_wait_press(void) BANKED;
void    ui_dialog(const char *l1, const char *l2, const char *l3) BANKED;

#endif
