"""Compare each stage of the assembly compression against Python."""
import re, sys
syms = {}
for line in open('build/testsha1.noi'):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m: syms[m.group(1)] = int(m.group(2), 16)
B = syms['_d_out']
from pyboy import PyBoy
pb = PyBoy('build/testsha1.gbc', window_type='headless', debug=False)
pb.set_emulation_speed(0)
rd = lambda o, n: bytes(pb.get_memory_value(B + o + i) for i in range(n))
f = 0
while f < 200000:
    pb.tick(); f += 1
    if rd(0, 1)[0] == 1: break
state = rd(8, 64)
wring = rd(72, 256)
vslots = rd(328, 128)
pb.stop(save=False)

# ---- Python reference, same algorithm ----
MASK = (1 << 64) - 1
def rotr(x, n): return ((x >> n) | (x << (64 - n))) & MASK
def shr(x, n): return x >> n
import math
def icbrt(n):
    x = int(round(n ** (1/3)))
    while x*x*x > n: x -= 1
    while (x+1)**3 <= n: x += 1
    return x
primes = []
n = 2
while len(primes) < 80:
    if all(n % p for p in primes if p*p <= n): primes.append(n)
    n += 1
K = [icbrt(p << 192) - (icbrt(p) << 64) for p in primes[:80]]
H0 = [math.isqrt(p << 128) - (math.isqrt(p) << 64) for p in primes[:8]]

block = bytes(range(1, 129))
W = [int.from_bytes(block[i*8:(i+1)*8], 'big') for i in range(16)]
exp_w = list(W)
h = list(H0)
a, b, c, d, e_, f_, g, hh = h
for i in range(80):
    if i >= 16:
        x, y = W[(i+1) % 16], W[(i+14) % 16]
        s0 = rotr(x, 1) ^ rotr(x, 8) ^ shr(x, 7)
        s1 = rotr(y, 19) ^ rotr(y, 61) ^ shr(y, 6)
        W[i % 16] = (W[i % 16] + s0 + W[(i+9) % 16] + s1) & MASK
    S1 = rotr(e_, 14) ^ rotr(e_, 18) ^ rotr(e_, 41)
    ch = (e_ & f_) ^ (~e_ & MASK & g)
    t1 = (hh + S1 + ch + K[i] + W[i % 16]) & MASK
    S0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39)
    maj = (a & b) ^ (a & c) ^ (b & c)
    t2 = (S0 + maj) & MASK
    hh, g, f_ = g, f_, e_
    e_ = (d + t1) & MASK
    d, c, b = c, b, a
    a = (t1 + t2) & MASK
final = [(x + y) & MASK for x, y in zip(H0, [a, b, c, d, e_, f_, g, hh])]

def le(v): return v.to_bytes(8, 'little')

# W as it stands after the 80 rounds: slot j holds W[64+j]
wrong_w = 0
for j in range(16):
    lo = wring[j*16:j*16+8]
    mir = wring[j*16+8:j*16+16]
    okk = lo == le(W[j])
    if not okk: wrong_w += 1
    if j < 4 or not okk:
        print('W ring[%2d]     rom %s  exp %s  %s%s'
              % (j, lo.hex(), le(W[j]).hex(), 'ok' if okk else 'WRONG',
                 '' if mir == lo else '  (mirror differs!)'))
print('W ring: %d of 16 slots wrong' % wrong_w)
print()
got = [int.from_bytes(state[i*8:(i+1)*8], 'little') for i in range(8)]
bad = 0
for i in range(8):
    okk = got[i] == final[i]
    if not okk: bad += 1
    print('h[%d]           rom %016x  exp %016x  %s'
          % (i, got[i], final[i], 'ok' if okk else 'WRONG'))
print()
print('all eight words match' if not bad else '%d of 8 words wrong' % bad)
