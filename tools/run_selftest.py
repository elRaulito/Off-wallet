"""Run the selftest ROM in PyBoy and check the crypto results it leaves in WRAM
against the official BIP-86 test vector. This proves the SDCC/Game Boy build
produces the same keys as the verified host build, not just that it compiles.

usage: python tools/run_selftest.py build/selftest.gbc build/selftest.noi
"""
import re, sys, os

ROM = sys.argv[1] if len(sys.argv) > 1 else 'build/selftest.gbc'
NOI = sys.argv[2] if len(sys.argv) > 2 else 'build/selftest.noi'
MAX_FRAMES = int(os.environ.get('MAX_FRAMES', '60000'))

EXP_SHA256_ABC = 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
EXP_SEED = ('5eb00bbddcf069084889a8ab9155568165f5c453ccb85e70811aaed6f6da5fc1'
            '9a5ac40b389cd370d086206dec8aa6c43daea6690f20ad3d8d48b2d2ce9e38e4')
EXP_OUTPUT_KEY = 'a60869f0dbcf1dc659c9cecbaf8050135ea9e8cdc487053f1dc6880949dc684c'
EXP_ADDRESS = 'bc1p5cyxnuxmeuwuvkwfem96lqzszd02n6xdcjrs20cac6yqjjwudpxqkedrcr'

syms = {}
for line in open(NOI):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m:
        syms[m.group(1)] = int(m.group(2), 16)
BASE = syms['_st_out']
print('st_out at 0x%04X' % BASE)

from pyboy import PyBoy
pyboy = PyBoy(ROM, window_type='headless', debug=False)
pyboy.set_emulation_speed(0)

def rd(off, n):
    return bytes(pyboy.get_memory_value(BASE + off + i) for i in range(n))

frames = 0
while frames < MAX_FRAMES:
    pyboy.tick()
    frames += 1
    if rd(0, 1)[0] == 1:
        break
    if frames % 1200 == 0:
        print('  ... %d frames (%.0fs of GB time), stage %d'
              % (frames, frames / 59.7, rd(1, 1)[0]))

done = rd(0, 1)[0]
stage = rd(1, 1)[0]
elapsed = int.from_bytes(rd(2, 2), 'little')
sha = rd(4, 32).hex()
seed = rd(36, 64).hex()
okey = rd(100, 32).hex()
addr = rd(132, 63).split(b'\0')[0].decode('ascii', 'replace')
pyboy.stop(save=False)

print('done=%d stage=%d after %d emulated frames (%.0fs of GB time)'
      % (done, stage, frames, frames / 59.7))
print('pbkdf2 took %d frames (%.0fs of GB time)' % (elapsed, elapsed / 59.7))

fails = 0
def chk(name, got, exp):
    global fails
    ok = got == exp
    if not ok:
        fails += 1
    print('%-12s %s' % (name, 'ok' if ok else 'FAIL'))
    if not ok:
        print('  got %s' % got)
        print('  exp %s' % exp)

if not done:
    print('ROM DID NOT FINISH within %d frames; partial results:' % MAX_FRAMES)
    print('  sha256 %s' % sha)
    print('  seed   %s' % seed)
    print('  okey   %s' % okey)
    sys.exit(2)

chk('sha256(abc)', sha, EXP_SHA256_ABC)
chk('bip39 seed', seed, EXP_SEED)
chk('output key', okey, EXP_OUTPUT_KEY)
chk('address', addr, EXP_ADDRESS)

print()
print('GAME BOY BUILD MATCHES THE OFFICIAL BIP-86 VECTOR' if not fails
      else '%d MISMATCHES' % fails)
sys.exit(1 if fails else 0)
