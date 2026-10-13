#ifndef OW_BANK_H
#define OW_BANK_H

/* Functions that live outside bank 0 are called through GBDK's banked
   trampoline on hardware, and are plain functions in the host test build. */
#ifdef OW_GAMEBOY
#include <gb/gb.h>
#define OW_BANKED BANKED
/* A function in bank 0 that maps a data bank at 0x4000 must put back whatever
   its caller had mapped: the caller may itself live in a switchable bank, and
   returning into an unmapped bank executes table data as code. */
#define OW_BANK_SAVE()    uint8_t ow_saved_bank = CURRENT_BANK
#define OW_BANK_RESTORE() SWITCH_ROM(ow_saved_bank)
#else
#define OW_BANKED
#define OW_BANK_SAVE()    (void)0
#define OW_BANK_RESTORE() (void)0
#endif

#endif
