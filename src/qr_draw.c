#pragma bank 6
/* Draws the encoded QR onto the background.
 *
 * 29 modules plus the 4-module quiet zone the spec requires is 37 modules; at
 * 3 pixels each that is 111 pixels, so the code occupies 14x14 tiles and each
 * of those 196 tiles is unique. That does not fit alongside the 95-glyph font
 * in one VRAM bank, so the tile data goes into VRAM bank 1 (a CGB has two) and
 * the map entries point at it with attribute bit 3. The font in bank 0 is
 * untouched, which is why the caption still works.
 */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "qr.h"
#include "qr_draw.h"

#define QUIET      4
#define PX_PER_MOD 3
#define QR_TILES   14                  /* 14x14 tiles = 112 px >= 111 */
#define QR_TILE0   0                   /* tile indices in VRAM bank 1 */
#define ATTR_BANK1 0x08

/* pixel column/row -> module index, or 0xFF for the quiet zone */
static uint8_t p2m[QR_TILES * 8];

void qr_draw(uint8_t ox, uint8_t oy) BANKED
{
    uint8_t buf[16];
    uint8_t row[QR_TILES];
    uint8_t tx, ty, py, px, i;

    for (i = 0; i < QR_TILES * 8; i++) {
        uint8_t m = (uint8_t)(i / PX_PER_MOD);
        p2m[i] = (uint8_t)((m >= QUIET && m < QUIET + QR_SIZE)
                           ? (m - QUIET) : 0xFF);
    }

    /* Writing 196 tiles takes longer than a VBlank, so blank the screen while
       we do it rather than tearing. */
    DISPLAY_OFF;

    VBK_REG = 1;                       /* tile data into VRAM bank 1 */
    for (ty = 0; ty < QR_TILES; ty++) {
        for (tx = 0; tx < QR_TILES; tx++) {
            for (py = 0; py < 8; py++) {
                uint8_t bits = 0;
                uint8_t my = p2m[(uint8_t)(ty * 8 + py)];
                if (my != 0xFF) {
                    for (px = 0; px < 8; px++) {
                        uint8_t mx = p2m[(uint8_t)(tx * 8 + px)];
                        if (mx != 0xFF && qr_get(mx, my))
                            bits |= (uint8_t)(0x80 >> px);
                    }
                }
                /* both bitplanes set: colour 3 of the palette */
                buf[py * 2] = bits;
                buf[py * 2 + 1] = bits;
            }
            set_bkg_data((uint8_t)(QR_TILE0 + ty * QR_TILES + tx), 1, buf);
        }
    }

    /* map entries: indices in bank 0, attributes (with the bank bit) in bank 1 */
    VBK_REG = 0;
    for (ty = 0; ty < QR_TILES; ty++) {
        for (tx = 0; tx < QR_TILES; tx++)
            row[tx] = (uint8_t)(QR_TILE0 + ty * QR_TILES + tx);
        set_bkg_tiles(ox, (uint8_t)(oy + ty), QR_TILES, 1, row);
    }
    VBK_REG = 1;
    for (tx = 0; tx < QR_TILES; tx++) row[tx] = ATTR_BANK1;   /* palette 0 */
    for (ty = 0; ty < QR_TILES; ty++)
        set_bkg_tiles(ox, (uint8_t)(oy + ty), QR_TILES, 1, row);
    VBK_REG = 0;

    DISPLAY_ON;
}
