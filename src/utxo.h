#ifndef OW_UTXO_H
#define OW_UTXO_H
#include <stdint.h>
#include <gb/gb.h>

#define UTXO_MAX 8

/* One unspent output the wallet can spend, typed in by hand. */
typedef struct {
    uint8_t  txid[32];   /* as a block explorer shows it */
    uint8_t  vout;
    uint32_t value;      /* satoshis */
    uint8_t  used;
} utxo_t;

extern utxo_t  utxo_tab[UTXO_MAX];
extern uint8_t utxo_count;

void     utxo_load(void) BANKED;
void     utxo_save(void) BANKED;
uint32_t utxo_total(void) BANKED;
void     utxo_format_btc(char *dst, uint32_t sats) BANKED;
void     utxo_format_sats(char *dst, uint32_t sats) BANKED;
void     utxo_screen(void) BANKED;

#endif
