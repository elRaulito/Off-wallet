/* Entry point. Kept tiny and in bank 0 (which the crypto fills) so the scene
   code and its strings can live in bank 1. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "game.h"

void main(void)
{
    if (_cpu == CGB_TYPE) cpu_fast();   /* 8.4 MHz: halves every wait below */
    game_run();
}
