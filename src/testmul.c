/* Test ROM for the hand-written SM83 multiply.

   Runs fe_mul_core (assembly) against a plain C schoolbook 256x256 multiply on
   pseudo-random inputs, inside the emulator, and records mismatches plus the
   timing of each. Read out by tools/run_mul_test.py. */
#include <gb/gb.h>
#include <gb/cgb.h>

extern uint8_t mul_a[32], mul_b[32], mul_t[68];
void fe_mul_core(void);

/* [0] done, [1] rounds completed, [2] mismatches,
   [3..4] frames for 32 asm multiplies, [5..6] frames for 32 C multiplies,
   [7..70] first mismatching pair (asm then C, 32 bytes each, low half) */
uint8_t tm_out[80];

static uint8_t ref[68];
static volatile uint16_t frames;
static void tick(void) { frames++; }

/* reference: the same schoolbook product the C build uses */
static void mul_ref(void)
{
    uint8_t i, j, carry, ai;
    uint16_t acc;
    for (i = 0; i < 68; i++) ref[i] = 0;
    for (i = 0; i < 32; i++) {
        ai = mul_a[i];
        if (!ai) continue;
        carry = 0;
        for (j = 0; j < 32; j++) {
            acc = (uint16_t)ref[i + j] + (uint16_t)ai * (uint16_t)mul_b[j] + carry;
            ref[i + j] = (uint8_t)acc;
            carry = (uint8_t)(acc >> 8);
        }
        ref[i + 32] = carry;
    }
}

static uint16_t lcg;
static uint8_t rnd(void)
{
    lcg = (uint16_t)(lcg * 25173u + 13849u);
    return (uint8_t)(lcg >> 8);
}

void main(void)
{
    uint8_t round, i, bad = 0;
    uint16_t t_asm = 0, t_c = 0;

    if (_cpu == CGB_TYPE) cpu_fast();
    for (i = 0; i < sizeof(tm_out); i++) tm_out[i] = 0;
    add_VBL(tick);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();
    DISPLAY_ON;

    lcg = 0x1234;
    for (round = 0; round < 32; round++) {
        for (i = 0; i < 32; i++) { mul_a[i] = rnd(); mul_b[i] = rnd(); }
        /* a few structured cases too */
        if (round == 0)  { for (i = 0; i < 32; i++) { mul_a[i] = 0xff; mul_b[i] = 0xff; } }
        if (round == 1)  { for (i = 0; i < 32; i++) { mul_a[i] = 0; mul_b[i] = 0xff; } mul_a[0] = 1; }
        if (round == 2)  { for (i = 0; i < 32; i++) { mul_a[i] = 0; mul_b[i] = 0; }
                           mul_a[31] = 0x80; mul_b[31] = 0x80; }

        frames = 0;
        fe_mul_core();
        t_asm += frames;

        frames = 0;
        mul_ref();
        t_c += frames;

        for (i = 0; i < 64; i++) {
            if (mul_t[i] != ref[i]) {
                if (!bad) {
                    uint8_t j;
                    for (j = 0; j < 32; j++) {
                        tm_out[7 + j] = mul_t[j];
                        tm_out[39 + j] = ref[j];
                    }
                }
                bad++;
                break;
            }
        }
        tm_out[1] = (uint8_t)(round + 1);
        tm_out[2] = bad;
    }

    tm_out[3] = (uint8_t)t_asm;  tm_out[4] = (uint8_t)(t_asm >> 8);
    tm_out[5] = (uint8_t)t_c;    tm_out[6] = (uint8_t)(t_c >> 8);
    tm_out[0] = 1;
    while (1) vsync();
}
