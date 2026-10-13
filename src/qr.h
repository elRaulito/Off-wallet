#ifndef OW_QR_H
#define OW_QR_H
#include <stdint.h>
#include "crypto/ow_bank.h"

/* QR encoder, just enough for a bech32m address: version 3 (29x29 modules),
   alphanumeric mode, error correction level L (77 characters of capacity, an
   address is 62). Everything is computed at run time -- the Galois field
   tables, the generator polynomial, the format bits -- so this costs no ROM
   tables and about 1 KB of RAM.

   qr_mod holds one byte per module:
     bit 0 = the module is dark
     bit 1 = the module is part of a function pattern and must not be masked */
#define QR_VERSION   3
#define QR_SIZE      29
#define QR_MODULES   (QR_SIZE * QR_SIZE)
#define QR_TOTAL_CW  70
#define QR_DATA_CW   55
#define QR_ECC_CW    15
#define QR_CAPACITY  77          /* alphanumeric characters at version 3-L */

#define QR_DARK      0x01
#define QR_FIXED     0x02

extern uint8_t qr_mod[QR_MODULES];

/* Encode an uppercase alphanumeric string. Returns 0 if it does not fit or
   contains a character the alphanumeric mode cannot express. */
uint8_t qr_encode(const char *text) OW_BANKED;

/* 1 when the module at (x, y) is dark. */
uint8_t qr_get(uint8_t x, uint8_t y) OW_BANKED;

/* The mask pattern chosen by the penalty rules, for tests. */
extern uint8_t qr_mask_used;

#endif
