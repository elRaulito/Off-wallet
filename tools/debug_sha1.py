"""Run one SHA-512 round in the assembly and check every intermediate."""
import re, sys, math
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
state = rd(8, 64); wring = rd(72, 256); vslots = rd(328, 128)
t1 = rd(456, 8); s0 = rd(464, 8); s1 = rd(472, 8); x = rd(480, 8)
pb.stop(save=False)

MASK = (1 << 64) - 1
def rotr(v, n): return ((v >> n) | (v << (64 - n))) & MASK
def icbrt(n):
    y = int(round(n ** (1/3)))
    while y*y*y > n: y -= 1
    while (y+1)**3 <= n: y += 1
    return y
primes, n = [], 2
while len(primes) < 80:
    if all(n % p for p in primes if p*p <= n): primes.append(n)
    n += 1
K = [icbrt(p << 192) - (icbrt(p) << 64) for p in primes[:80]]
H0 = [math.isqrt(p << 128) - (math.isqrt(p) << 64) for p in primes[:8]]

block = bytes(range(1, 129))
W = [int.from_bytes(block[i*8:(i+1)*8], 'big') for i in range(16)]
a, b, c, d, e_, f_, g, hh = H0
S1 = rotr(e_, 14) ^ rotr(e_, 18) ^ rotr(e_, 41)
ch = (e_ & f_) ^ (~e_ & MASK & g)
T1 = (hh + S1 + ch + K[0] + W[0]) & MASK
S0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39)
maj = (a & b) ^ (a & c) ^ (b & c)
T2 = (S0 + maj) & MASK
e_new = (d + T1) & MASK
a_new = (T1 + T2) & MASK

def num(bs): return int.from_bytes(bs, 'little')
def show(name, got, exp):
    ok = got == exp
    print('%-14s rom %016x  exp %016x  %s' % (name, got, exp, 'ok' if ok else 'WRONG'))
    return ok

print('after a single round, with the block 01 02 03 ... 80:')
print()
allok = True
allok &= show('W[0]', num(wring[0:8]), W[0])
allok &= show('Sum1(e)', num(s1), S1)
allok &= show('Ch(e,f,g)', num(x), maj)          # x holds Maj by the end
allok &= show('T1', num(t1), T1)
allok &= show('Sum0+Maj (T2)', num(s0), T2)
print()
# slot 3 became e', slot 7 became a'
allok &= show("e' (slot 3)", num(vslots[3*16:3*16+8]), e_new)
allok &= show("a' (slot 7)", num(vslots[7*16:7*16+8]), a_new)
print()
print('every intermediate matches' if allok else 'the first WRONG line above is the bug')
