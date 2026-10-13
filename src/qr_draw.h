#ifndef OW_QR_DRAW_H
#define OW_QR_DRAW_H
#include <stdint.h>
#include <gb/gb.h>

/* Draw the QR encoded by qr_encode() at tile position (ox, oy). Occupies
   14x14 tiles and briefly blanks the display while uploading them. */
void qr_draw(uint8_t ox, uint8_t oy) BANKED;

#endif
