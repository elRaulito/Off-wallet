#pragma bank 1
/* Draws the screens and plays the conversations defined in content/story.txt.
   Lives in bank 1 next to the generated tables and the scene code, so reading
   them needs no bank switching. */
#include <gb/gb.h>
#include "story.h"
#include "ui.h"
#include "gfx/gfx_tiles.h"

void story_screen_draw(uint8_t id) BANKED
{
    const story_screen *sc = &STORY_SCREENS[id];
    uint8_t i;

    if (sc->clear) ui_clear();
    for (i = 0; i < sc->count; i++) {
        uint8_t k = (uint8_t)(sc->first + i);
        const story_op *op = &STORY_OPS[k];
        switch (op->kind) {
        case SOP_TEXT:
            ui_text(op->x, op->y, op->s);
            if (op->arg) {
                uint8_t n = 0;
                while (op->s[n]) n++;
                ui_attr(op->x, op->y, n, op->arg);
            }
            break;
        case SOP_SPRITE:
            /* tile base and size live in parallel tables so that story_op
               stays small; arg is the palette */
            ui_blit(op->x, op->y,
                    (uint8_t)(STORY_OP_SIZE[k] >> 4),
                    (uint8_t)(STORY_OP_SIZE[k] & 15),
                    STORY_OP_TILE[k], op->arg);
            break;
        case SOP_BLANK:
            ui_blank(op->x, op->y, op->arg);
            break;
        default:
            break;
        }
    }
}

/* Name plate above the dialogue box, Pokemon style. Row 11 is the line just
   above the box ui_dialog draws at rows 12..17. */
#define NAME_ROW 11

void story_dialog_run(uint8_t id) BANKED
{
    const story_dialog *dl = &STORY_DIALOGS[id];
    uint8_t i;
    uint8_t last_speaker = 255;

    for (i = 0; i < dl->count; i++) {
        const story_box *b = &STORY_BOXES[dl->first + i];
        if (b->speaker != 255) {
            const story_char *ch = &STORY_CHARS[b->speaker];
            if (b->speaker != last_speaker) {
                ui_blit(ch->px, ch->py, (uint8_t)(ch->size >> 4),
                        (uint8_t)(ch->size & 15), ch->tile, ch->pal);
            }
            ui_blank(0, NAME_ROW, 20);
            if (ch->name) {
                ui_text(1, NAME_ROW, ch->name);
                ui_attr(1, NAME_ROW, 18, PAL_ACCENT);
            }
        } else {
            ui_blank(0, NAME_ROW, 20);
        }
        last_speaker = b->speaker;
        ui_dialog(b->l1, b->l2, b->l3);
    }
}
