"""Verify the crypto stack on emulated hardware and print timings.

Reads the tf_out buffer left by src/testfe.c: correctness bitmap, frame
counts per primitive, and the first bytes of the BIP-86 derived output key."""
import re, sys

ROM = 'build/testfe.gbc'
syms = {}
for line in open('build/testfe.noi'):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m:
        syms[m.group(1)] = int(m.group(2), 16)
B = syms['_tf_out']

from pyboy import PyBoy
pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
rd = lambda o, n=1: bytes(pb.get_memory_value(B + o + i) for i in range(n))

f = 0
while f < 600000:
    pb.tick(); f += 1
    if rd(0)[0] == 1:
        break
p = rd(1)[0]
t = [int.from_bytes(rd(2 + i * 2, 2), 'little') for i in range(6)]
okey = rd(16, 8).hex()
pb.stop(save=False)

FPS = 59.7
print('correctness:')
for i, n in enumerate(['fe_mul', 'fe_sqr', 'fe_inv', 'k*G', 'BIP-86 derivation']):
    print('  %-18s %s' % (n, 'ok' if p & (1 << i) else 'FAIL'))
print('timings (Game Boy time):')
print('  fe_mul              %7.1f ms' % (t[0] / FPS / 16 * 1000))
print('  fe_sqr              %7.1f ms' % (t[1] / FPS / 16 * 1000))
print('  fe_inv              %7.2f s' % (t[2] / FPS))
print('  k*G                 %7.2f s' % (t[3] / FPS))
print('  sha512 (1 block)    %7.1f ms' % (t[4] / FPS / 8 * 1000))
print('  BIP-32 + taproot    %7.1f s' % (t[5] / FPS))
print()
print('derived output key starts %s (expect a60869f0dbcf1dc6)' % okey)
pbkdf2 = t[4] / 8 * 4096 / FPS          # 4096 one-block compressions
print('estimated PBKDF2 (4096 compressions): %.0f s = %.1f min' % (pbkdf2, pbkdf2 / 60))
print('estimated ANCIENT hatch: %.1f min' % ((pbkdf2 + t[5] / FPS) / 60))
print()
ok = p == 0x1f
print('ALL CHECKS PASS' if ok else 'FAILURES PRESENT (bitmap 0x%02X)' % p)
sys.exit(0 if ok else 1)
