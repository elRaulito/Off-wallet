#ifndef OW_STORY_H
#define OW_STORY_H
#include <stdint.h>
#include <gb/gb.h>
#include "story_data.h"

/* Screens and dialogue come from content/story.txt, compiled by
   tools/gen/gen_story.py into story_data.c. Nothing here knows any text. */

#define SOP_END     0
#define SOP_TEXT    1
#define SOP_SPRITE  2
#define SOP_BLANK   3

typedef struct {
    uint8_t     kind;
    uint8_t     x, y;
    uint8_t     arg;     /* TEXT: palette; BLANK: width */
    const char *s;
} story_op;

typedef struct {
    uint8_t first;       /* index into STORY_OPS */
    uint8_t count;
    uint8_t clear;       /* wipe the screen first */
} story_screen;

typedef struct {
    const char *name;    /* 0 when the speaker is unnamed */
    uint8_t     tile;    /* first tile of the portrait */
    uint8_t     pal;
    uint8_t     px, py;  /* where the portrait is drawn */
    uint8_t     size;    /* tiles: w<<4 | h */
} story_char;

typedef struct {
    uint8_t     speaker; /* index into STORY_CHARS, 255 = narration */
    const char *l1;
    const char *l2;
    const char *l3;
} story_box;

typedef struct {
    uint8_t first;       /* index into STORY_BOXES */
    uint8_t count;
} story_dialog;

extern const story_char   STORY_CHARS[];
extern const story_op     STORY_OPS[];
extern const uint8_t      STORY_OP_TILE[];
extern const uint8_t      STORY_OP_SIZE[];   /* tiles: w<<4 | h */
extern const story_screen STORY_SCREENS[];
extern const story_box    STORY_BOXES[];
extern const story_dialog STORY_DIALOGS[];
extern const uint16_t     STORY_PALETTES[];
extern const uint8_t      STORY_PALETTE_FIRST;
extern const uint8_t      STORY_PALETTE_COUNT;
extern const uint8_t      STORY_TILES[];

/* Where the custom sprite tiles land in VRAM, after the font and the
   built-in graphics. */
#define STORY_TILE_BASE (GFX_BASE + GFX_NTILES)

void story_screen_draw(uint8_t id) BANKED;
void story_dialog_run(uint8_t id) BANKED;

#endif
