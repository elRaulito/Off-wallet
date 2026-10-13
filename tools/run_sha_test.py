"""Compare the SM83 assembly SHA-512 with the C one, on emulated hardware."""
import re, sys, os
ROM = 'build/testsha.gbc'
syms = {}
for line in open('build/testsha.noi'):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m:
        syms[m.group(1)] = int(m.group(2), 16)
B = syms['_ts_out']
from pyboy import PyBoy
pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
rd = lambda o, n=1: bytes(pb.get_memory_value(B + o + i) for i in range(n))
f = 0
last = -1
while f < int(os.environ.get('MAX_FRAMES', '900000')):
    pb.tick(); f += 1
    if rd(0)[0] == 1:
        break
    if f % 3000 == 0:
        n = rd(1)[0]
        if n != last:
            last = n
            print('  %d blocks compared (%d frames)' % (n, f))
done = rd(0)[0]
blocks, bad = rd(1)[0], rd(2)[0]
t_c = int.from_bytes(rd(3, 2), 'little')
t_a = int.from_bytes(rd(5, 2), 'little')
vec_ok = rd(7)[0]
dc, da = rd(8, 32).hex(), rd(40, 32).hex()
seed16 = rd(64, 16).hex()
pb.stop(save=False)

FPS = 59.7
print()
print('finished          %s' % ('yes' if done else 'NO (timed out)'))
print('blocks compared   %d' % blocks)
print('mismatches        %d' % bad)
if bad:
    print('  C   %s' % dc)
    print('  asm %s' % da)
print()
print('C   compression   %7.1f ms' % (t_c / FPS / 8 * 1000))
print('asm compression   %7.1f ms' % (t_a / FPS / 8 * 1000))
if t_a:
    print('speed-up          %7.1fx' % (t_c / float(t_a)))
    pbk = t_a / 8.0 * 4096 / FPS
    print()
    print('PBKDF2 (4096 compressions): %.0f s = %.1f min' % (pbk, pbk / 60))
    print('ANCIENT hatch, with the 158 s of curve work: %.1f min'
          % ((pbk + 158) / 60))
print()
print('hmac/pbkdf2 path ran: %s (seed starts %s)' % ('yes' if vec_ok else 'no', seed16))
ok = done and blocks == 24 and bad == 0
print()
print('THE ASSEMBLY MATCHES THE C IMPLEMENTATION' if ok else 'FAILURES PRESENT')
sys.exit(0 if ok else 1)
