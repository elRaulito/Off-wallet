#ifndef OW_PROGRESS_H
#define OW_PROGRESS_H
#include <stdint.h>

/* All of these live in bank 0; see progress.c for why. */
extern volatile uint16_t ow_anim;
extern volatile uint8_t  ow_anim_on;
extern volatile uint8_t  ow_progress;
extern volatile uint8_t  ow_progress_max;

void ow_vbl(void);
void ow_load_gfx(void);
void ow_bump(void);
void ow_progress_begin(uint8_t total_steps, uint8_t divisor);
void ow_progress_end(void);

#endif
