/* Print the QR matrix for a few strings so check_qr.py can diff it
   against segno. */
#include <stdio.h>
#include "../src/qr.h"

static const char *CASES[] = {
    "BC1PQX2PT3AWH5A5W3ZVDV7K3T3UKZA36WRTFMJYHPXRREGQQ6C8DZ0QA5PG3C",
    "BC1P5CYXNUXMEUWUVKWFEM96LQZSZD02N6XDCJRS20CAC6YQJJWUDPXQKEDRCR",
    "BC1PRHXEGLMK38M9HA8R73PKV9ZPV7WJXNFNV7ZYVTZD8V37W2PG4TPSCEMKZ5",
    "HELLO WORLD",
    "0123456789",
};

int main(void)
{
    for (unsigned c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        if (!qr_encode(CASES[c])) {
            printf("FAIL|%s\n", CASES[c]);
            continue;
        }
        printf("CASE|%d|%s\n", qr_mask_used, CASES[c]);
        for (int y = 0; y < QR_SIZE; y++) {
            for (int x = 0; x < QR_SIZE; x++) putchar(qr_get(x, y) ? '1' : '0');
            putchar('\n');
        }
    }
    return 0;
}
