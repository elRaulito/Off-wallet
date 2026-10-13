#ifdef OW_GAMEBOY
#pragma bank 6
#endif
/* Version 3-L QR encoder. See qr.h for the layout of qr_mod.
 *
 * Deliberately host-compilable, so test/check_qr.py can compare the entire
 * module matrix against the segno library for the same input. That checks the
 * bitstream, the Reed-Solomon codewords, the module placement, the mask chosen
 * by the penalty rules and the format bits all at once.
 */
#include "qr.h"

uint8_t qr_mod[QR_MODULES];
uint8_t qr_mask_used;

static uint8_t cw[QR_TOTAL_CW];      /* data codewords, then ECC codewords */
static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static uint16_t bitpos;

/* --------------------------------------------------------------- GF(256) */

static void gf_init(void)
{
    uint16_t i;
    uint8_t x = 1;
    for (i = 0; i < 255; i++) {
        gf_exp[i] = x;
        gf_log[x] = (uint8_t)i;
        /* x *= 2 modulo the QR primitive polynomial 0x11d */
        x = (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1d : 0x00));
    }
    for (i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0;
}

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
    if (!a || !b) return 0;
    return gf_exp[(uint16_t)gf_log[a] + (uint16_t)gf_log[b]];
}

/* ------------------------------------------------ alphanumeric bitstream */

/* position of c in "0-9 A-Z space $%*+-./:" , or 255 if it has none */
static uint8_t alnum(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'Z') return (uint8_t)(c - 'A' + 10);
    switch (c) {
    case ' ': return 36;
    case '$': return 37;
    case '%': return 38;
    case '*': return 39;
    case '+': return 40;
    case '-': return 41;
    case '.': return 42;
    case '/': return 43;
    case ':': return 44;
    default:  return 255;
    }
}

static void put_bits(uint16_t value, uint8_t nbits)
{
    while (nbits--) {
        if ((value >> nbits) & 1)
            cw[bitpos >> 3] |= (uint8_t)(0x80 >> (bitpos & 7));
        bitpos++;
    }
}

/* ---------------------------------------------------------- module grid */

static void set_mod(uint8_t x, uint8_t y, uint8_t dark, uint8_t fixed)
{
    qr_mod[(uint16_t)y * QR_SIZE + x] =
        (uint8_t)((dark ? QR_DARK : 0) | (fixed ? QR_FIXED : 0));
}

uint8_t qr_get(uint8_t x, uint8_t y) OW_BANKED
{
    return (uint8_t)(qr_mod[(uint16_t)y * QR_SIZE + x] & QR_DARK);
}

static uint8_t is_dark(int8_t x, int8_t y)
{
    if (x < 0 || y < 0 || x >= QR_SIZE || y >= QR_SIZE) return 0;
    return (uint8_t)(qr_mod[(uint16_t)y * QR_SIZE + x] & QR_DARK);
}

static void finder(int8_t ox, int8_t oy)
{
    int8_t dx, dy;
    for (dy = -1; dy <= 7; dy++) {
        for (dx = -1; dx <= 7; dx++) {
            int8_t x = (int8_t)(ox + dx), y = (int8_t)(oy + dy);
            uint8_t dark;
            if (x < 0 || y < 0 || x >= QR_SIZE || y >= QR_SIZE) continue;
            if (dx < 0 || dy < 0 || dx > 6 || dy > 6) {
                dark = 0;                                   /* separator */
            } else {
                uint8_t ax = (uint8_t)(dx < 3 ? dx : 6 - dx);
                uint8_t ay = (uint8_t)(dy < 3 ? dy : 6 - dy);
                uint8_t r = (ax < ay) ? ax : ay;
                dark = (uint8_t)(r != 1);                   /* ring 1 is light */
            }
            set_mod((uint8_t)x, (uint8_t)y, dark, 1);
        }
    }
}

static void function_patterns(void)
{
    uint16_t i;
    for (i = 0; i < QR_MODULES; i++) qr_mod[i] = 0;

    finder(0, 0);
    finder(QR_SIZE - 7, 0);
    finder(0, QR_SIZE - 7);

    for (i = 8; i < QR_SIZE - 8; i++) {
        set_mod((uint8_t)i, 6, (uint8_t)(!(i & 1)), 1);
        set_mod(6, (uint8_t)i, (uint8_t)(!(i & 1)), 1);
    }

    /* version 3 places exactly one alignment pattern, centred at (22,22) */
    {
        int8_t dx, dy;
        for (dy = -2; dy <= 2; dy++) {
            for (dx = -2; dx <= 2; dx++) {
                uint8_t ax = (uint8_t)(dx < 0 ? -dx : dx);
                uint8_t ay = (uint8_t)(dy < 0 ? -dy : dy);
                uint8_t r = (ax > ay) ? ax : ay;
                set_mod((uint8_t)(22 + dx), (uint8_t)(22 + dy),
                        (uint8_t)(r != 1), 1);
            }
        }
    }

    /* reserve the format-information areas and set the always-dark module */
    for (i = 0; i <= 8; i++) {
        if (i != 6) {
            set_mod((uint8_t)i, 8, 0, 1);
            set_mod(8, (uint8_t)i, 0, 1);
        }
    }
    for (i = 0; i < 8; i++) {
        set_mod((uint8_t)(QR_SIZE - 1 - i), 8, 0, 1);
        set_mod(8, (uint8_t)(QR_SIZE - 1 - i), 0, 1);
    }
    set_mod(8, QR_SIZE - 8, 1, 1);
}

static void place_data(void)
{
    int8_t dir = -1;                 /* -1 = upwards */
    int8_t col = QR_SIZE - 1;
    int8_t row = QR_SIZE - 1;
    uint16_t bit = 0;

    while (col > 0) {
        if (col == 6) col--;         /* the vertical timing pattern */
        for (;;) {
            int8_t c;
            for (c = 0; c < 2; c++) {
                uint8_t x = (uint8_t)(col - c);
                uint16_t idx = (uint16_t)row * QR_SIZE + x;
                if (!(qr_mod[idx] & QR_FIXED)) {
                    uint8_t d = 0;
                    if (bit < (uint16_t)QR_TOTAL_CW * 8)
                        d = (uint8_t)((cw[bit >> 3] >> (7 - (bit & 7))) & 1);
                    qr_mod[idx] = (uint8_t)(d ? QR_DARK : 0);
                    bit++;
                }
            }
            row = (int8_t)(row + dir);
            if (row < 0 || row >= QR_SIZE) {
                row = (int8_t)(row - dir);
                dir = (int8_t)(-dir);
                break;
            }
        }
        col = (int8_t)(col - 2);
    }
}

/* ------------------------------------------------------------- masking */

static uint8_t mask_bit(uint8_t m, uint8_t i, uint8_t j)
{
    uint16_t ij = (uint16_t)i * j;
    switch (m) {
    case 0:  return (uint8_t)(((i + j) % 2) == 0);
    case 1:  return (uint8_t)((i % 2) == 0);
    case 2:  return (uint8_t)((j % 3) == 0);
    case 3:  return (uint8_t)(((i + j) % 3) == 0);
    case 4:  return (uint8_t)((((i / 2) + (j / 3)) % 2) == 0);
    case 5:  return (uint8_t)(((ij % 2) + (ij % 3)) == 0);
    case 6:  return (uint8_t)((((ij % 2) + (ij % 3)) % 2) == 0);
    default: return (uint8_t)(((((i + j) % 2) + (ij % 3)) % 2) == 0);
    }
}

static void apply_mask(uint8_t m)
{
    uint8_t i, j;
    for (i = 0; i < QR_SIZE; i++) {
        for (j = 0; j < QR_SIZE; j++) {
            uint16_t idx = (uint16_t)i * QR_SIZE + j;
            if (qr_mod[idx] & QR_FIXED) continue;
            if (mask_bit(m, i, j)) qr_mod[idx] ^= QR_DARK;
        }
    }
}

/* --------------------------------------------------------- format info */

static void put_format(uint8_t ecc_bits, uint8_t mask)
{
    uint16_t data = (uint16_t)((ecc_bits << 3) | mask);
    uint16_t rem = data;
    uint16_t bits;
    uint8_t i;

    for (i = 0; i < 10; i++)
        rem = (uint16_t)((rem << 1) ^ (((rem >> 9) & 1) ? 0x537 : 0));
    bits = (uint16_t)((((uint16_t)data << 10) | (rem & 0x3ff)) ^ 0x5412);

    /* copy 1, around the top-left finder */
    for (i = 0; i <= 5; i++)
        set_mod(8, i, (uint8_t)((bits >> i) & 1), 1);
    set_mod(8, 7, (uint8_t)((bits >> 6) & 1), 1);
    set_mod(8, 8, (uint8_t)((bits >> 7) & 1), 1);
    set_mod(7, 8, (uint8_t)((bits >> 8) & 1), 1);
    for (i = 9; i <= 14; i++)
        set_mod((uint8_t)(14 - i), 8, (uint8_t)((bits >> i) & 1), 1);

    /* copy 2, split between the other two finders */
    for (i = 0; i <= 7; i++)
        set_mod((uint8_t)(QR_SIZE - 1 - i), 8, (uint8_t)((bits >> i) & 1), 1);
    for (i = 8; i <= 14; i++)
        set_mod(8, (uint8_t)(QR_SIZE - 15 + i), (uint8_t)((bits >> i) & 1), 1);
    set_mod(8, QR_SIZE - 8, 1, 1);
}

/* ------------------------------------------------------------ penalties */

static uint16_t penalty(void)
{
    uint16_t score = 0;
    uint8_t i, j;
    uint16_t dark = 0;

    /* rule 1: runs of five or more of the same colour */
    for (i = 0; i < QR_SIZE; i++) {
        uint8_t run = 1, prev = is_dark(0, (int8_t)i);
        for (j = 1; j < QR_SIZE; j++) {
            uint8_t v = is_dark((int8_t)j, (int8_t)i);
            if (v == prev) {
                run++;
            } else {
                if (run >= 5) score = (uint16_t)(score + 3 + (run - 5));
                run = 1;
                prev = v;
            }
        }
        if (run >= 5) score = (uint16_t)(score + 3 + (run - 5));

        run = 1; prev = is_dark((int8_t)i, 0);
        for (j = 1; j < QR_SIZE; j++) {
            uint8_t v = is_dark((int8_t)i, (int8_t)j);
            if (v == prev) {
                run++;
            } else {
                if (run >= 5) score = (uint16_t)(score + 3 + (run - 5));
                run = 1;
                prev = v;
            }
        }
        if (run >= 5) score = (uint16_t)(score + 3 + (run - 5));
    }

    /* rule 2: 2x2 blocks of one colour */
    for (i = 0; i + 1 < QR_SIZE; i++) {
        for (j = 0; j + 1 < QR_SIZE; j++) {
            uint8_t v = is_dark((int8_t)j, (int8_t)i);
            if (v == is_dark((int8_t)(j + 1), (int8_t)i) &&
                v == is_dark((int8_t)j, (int8_t)(i + 1)) &&
                v == is_dark((int8_t)(j + 1), (int8_t)(i + 1)))
                score = (uint16_t)(score + 3);
        }
    }

    /* rule 3: the finder-like 1:1:3:1:1 pattern with four light modules */
    for (i = 0; i < QR_SIZE; i++) {
        for (j = 0; j < QR_SIZE; j++) {
            uint8_t k;
            uint8_t rowpat = 1, colpat = 1;
            static const uint8_t PAT[11] =
                { 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0 };
            if (j + 11 <= QR_SIZE) {
                for (k = 0; k < 11; k++) {
                    if (is_dark((int8_t)(j + k), (int8_t)i) != PAT[k]) { rowpat = 0; break; }
                }
                if (rowpat) score = (uint16_t)(score + 40);
                rowpat = 1;
                for (k = 0; k < 11; k++) {
                    if (is_dark((int8_t)(j + k), (int8_t)i) != PAT[10 - k]) { rowpat = 0; break; }
                }
                if (rowpat) score = (uint16_t)(score + 40);
            }
            if (i + 11 <= QR_SIZE) {
                for (k = 0; k < 11; k++) {
                    if (is_dark((int8_t)j, (int8_t)(i + k)) != PAT[k]) { colpat = 0; break; }
                }
                if (colpat) score = (uint16_t)(score + 40);
                colpat = 1;
                for (k = 0; k < 11; k++) {
                    if (is_dark((int8_t)j, (int8_t)(i + k)) != PAT[10 - k]) { colpat = 0; break; }
                }
                if (colpat) score = (uint16_t)(score + 40);
            }
        }
    }

    /* rule 4: how far the dark proportion is from half.
       The counter must be 16-bit: there are 841 modules. */
    {
        uint16_t k;
        for (k = 0; k < QR_MODULES; k++)
            if (qr_mod[k] & QR_DARK) dark++;
    }
    {
        uint16_t pct = (uint16_t)((uint32_t)dark * 100 / QR_MODULES);
        uint16_t off = (pct > 50) ? (uint16_t)(pct - 50) : (uint16_t)(50 - pct);
        score = (uint16_t)(score + (off / 5) * 10);
    }
    return score;
}

/* ------------------------------------------------------------- encoding */

uint8_t qr_encode(const char *text) OW_BANKED
{
    uint16_t n = 0, i;
    uint8_t m, best = 0;
    uint16_t bestscore = 0;

    while (text[n]) n++;
    if (n > QR_CAPACITY) return 0;
    for (i = 0; i < n; i++)
        if (alnum(text[i]) == 255) return 0;

    gf_init();
    for (i = 0; i < QR_TOTAL_CW; i++) cw[i] = 0;
    bitpos = 0;

    put_bits(0x2, 4);                       /* alphanumeric mode */
    put_bits(n, 9);                         /* character count, versions 1-9 */
    for (i = 0; i + 1 < n; i += 2)
        put_bits((uint16_t)(45u * alnum(text[i]) + alnum(text[i + 1])), 11);
    if (n & 1)
        put_bits(alnum(text[n - 1]), 6);

    /* terminator, then pad bytes to fill the data capacity */
    {
        uint16_t cap = (uint16_t)QR_DATA_CW * 8;
        uint8_t term = (uint8_t)((cap - bitpos > 4) ? 4 : (cap - bitpos));
        put_bits(0, term);
        while (bitpos & 7) put_bits(0, 1);
        i = (uint16_t)(bitpos >> 3);
        while (i < QR_DATA_CW) {
            cw[i++] = 0xEC;
            if (i < QR_DATA_CW) cw[i++] = 0x11;
        }
    }

    /* Reed-Solomon: 15 check codewords over the 55 data codewords */
    {
        uint8_t gen[QR_ECC_CW + 1];
        uint8_t ecc[QR_ECC_CW];
        uint8_t j;

        for (i = 0; i <= QR_ECC_CW; i++) gen[i] = 0;
        gen[0] = 1;
        for (i = 0; i < QR_ECC_CW; i++) {
            /* multiply the generator by (x - alpha^i) */
            for (j = (uint8_t)(i + 1); j > 0; j--)
                gen[j] = (uint8_t)(gen[j - 1] ^ gf_mul(gen[j], gf_exp[i]));
            gen[0] = gf_mul(gen[0], gf_exp[i]);
        }
        for (i = 0; i < QR_ECC_CW; i++) ecc[i] = 0;
        for (i = 0; i < QR_DATA_CW; i++) {
            uint8_t factor = (uint8_t)(cw[i] ^ ecc[0]);
            for (j = 0; j + 1 < QR_ECC_CW; j++) ecc[j] = ecc[j + 1];
            ecc[QR_ECC_CW - 1] = 0;
            for (j = 0; j < QR_ECC_CW; j++)
                ecc[j] ^= gf_mul(gen[QR_ECC_CW - 1 - j], factor);
        }
        for (i = 0; i < QR_ECC_CW; i++) cw[QR_DATA_CW + i] = ecc[i];
    }

    function_patterns();
    place_data();

    /* pick the mask with the lowest penalty, format bits included */
    for (m = 0; m < 8; m++) {
        uint16_t s;
        apply_mask(m);
        put_format(1, m);                   /* 1 = error correction level L */
        s = penalty();
        apply_mask(m);                       /* xor is its own inverse */
        if (m == 0 || s < bestscore) {
            bestscore = s;
            best = m;
        }
    }
    apply_mask(best);
    put_format(1, best);
    qr_mask_used = best;
    return 1;
}
