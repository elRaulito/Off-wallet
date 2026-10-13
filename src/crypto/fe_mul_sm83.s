;; 256x256 -> 512 bit multiply for SM83 (Game Boy CPU).
;;
;; The SM83 has no multiply instruction, and SDCC's 8x8 helper costs a few
;; hundred cycles per product; at 1024 products per field multiply that made
;; this the whole program's bottleneck (165 ms each, measured on hardware).
;;
;; This routine never multiplies. It precomputes b shifted left by 0..7 bits
;; (33 bytes each) and then, for every set bit k of every byte a[i], adds the
;; k-th shifted copy into t at byte offset i. Each add is a carry-chained
;; 33-byte sequence, unrolled: ld a,(de) / adc a,(hl) / ld (hl+),a / inc de,
;; which is 8 machine cycles per byte with the carry flowing between bytes.
;;
;; Inputs and outputs are fixed buffers rather than arguments, to sidestep
;; calling conventions: mul_a, mul_b in, mul_t out (68 bytes, of which 64 are
;; significant; the tail is carry headroom).

        .module fe_mul_sm83
        .globl  _fe_mul_core, _mul_a, _mul_b, _mul_t

        .area   _DATA
_mul_a::
        .ds     32
_mul_b::
        .ds     32
_mul_t::
        .ds     68
bshift:
        .ds     264             ; 8 copies of b, 33 bytes each
cur_ai:
        .ds     1
cur_bs:
        .ds     2

        .area   _CODE

;; ---------------------------------------------------------------- setup
zero_t:
        ld      hl, #_mul_t
        ld      b, #68
        xor     a
zt_loop:
        ld      (hl+), a
        dec     b
        jr      nz, zt_loop
        ret

;; bshift[0] = mul_b, then a zero 33rd byte
copy_b0:
        ld      hl, #_mul_b
        ld      de, #bshift
        ld      b, #32
cb_loop:
        ld      a, (hl+)
        ld      (de), a
        inc     de
        dec     b
        jr      nz, cb_loop
        xor     a
        ld      (de), a
        ret

;; bshift[k] = bshift[k-1] << 1 for k = 1..7
build_shifts:
        ld      c, #7
        ld      hl, #bshift
        ld      de, #bshift+33
bs_outer:
        ld      b, #33
        xor     a               ; clears carry for the shift chain
bs_inner:
        ld      a, (hl+)
        rla                     ; carry in from previous byte, out to the next
        ld      (de), a
        inc     de
        dec     b               ; preserves carry
        jr      nz, bs_inner
        dec     c
        jr      nz, bs_outer
        ret

;; ---------------------------------------------------------------- add step
;; t[i..] += bshift[k]; HL -> mul_t+i on entry, DE -> bshift[k]
;; 33 bytes unrolled, then the remaining carry is rippled out.
.macro  ADDB
        ld      a, (de)
        adc     a, (hl)
        ld      (hl+), a
        inc     de
.endm

add33:
        xor     a               ; clear carry
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ADDB
        ret     nc
carry_prop:
        ld      a, (hl)
        add     a, #1
        ld      (hl+), a
        jr      c, carry_prop
        ret

;; ---------------------------------------------------------------- main
_fe_mul_core::
        call    zero_t
        call    copy_b0
        call    build_shifts

        ld      c, #0           ; c = i, the byte index into a
i_loop:
        ;; a[i]
        ld      hl, #_mul_a
        ld      b, #0
        ld      e, c
        ld      d, #0
        add     hl, de
        ld      a, (hl)
        or      a
        jr      z, i_next       ; a zero byte contributes nothing

        ld      (cur_ai), a
        ;; cur_bs = bshift; it advances by 33 per bit, so no k*33 multiply
        ld      hl, #bshift
        ld      a, l
        ld      (cur_bs), a
        ld      a, h
        ld      (cur_bs+1), a

k_loop:
        ;; shift the low bit of cur_ai into carry
        ld      a, (cur_ai)
        srl     a
        ld      (cur_ai), a
        jr      nc, k_next      ; bit clear: skip the add

        ;; DE -> bshift[k]
        ld      a, (cur_bs)
        ld      e, a
        ld      a, (cur_bs+1)
        ld      d, a
        ;; HL = mul_t + i
        ld      hl, #_mul_t
        ld      a, c
        add     a, l
        ld      l, a
        ld      a, #0
        adc     a, h
        ld      h, a
        call    add33

k_next:
        ;; cur_bs += 33
        ld      a, (cur_bs)
        add     a, #33
        ld      (cur_bs), a
        ld      a, (cur_bs+1)
        adc     a, #0
        ld      (cur_bs+1), a
        ;; any bits left in cur_ai?
        ld      a, (cur_ai)
        or      a
        jr      nz, k_loop

i_next:
        inc     c
        ld      a, c
        cp      #32
        jr      nz, i_loop
        ret
