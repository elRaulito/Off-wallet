"""Read the benchmark ROM's frame counters out of an emulator."""
import re, sys, os
ROM = sys.argv[1] if len(sys.argv) > 1 else 'build/bench.gbc'
NOI = ROM.rsplit('.',1)[0] + '.noi'
MAX = int(os.environ.get('MAX_FRAMES','120000'))
syms = {}
for line in open(NOI):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m: syms[m.group(1)] = int(m.group(2),16)
BASE = syms['_bn_out']
from pyboy import PyBoy
pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
rd = lambda o,n=1: bytes(pb.get_memory_value(BASE+o+i) for i in range(n))
LABELS = ['64 fe_mul','fe_inv','k*G','16 sha256','16 sha512','full derive']
f=0; last=-1
while f < MAX:
    pb.tick(); f+=1
    if rd(0)[0]==1: break
    st = rd(1)[0]
    if st != last:
        last = st
        print('  stage %d at %d frames' % (st, f))
print('finished=%d stage=%d' % (rd(0)[0], rd(1)[0]))
vals = [int.from_bytes(rd(2+i*2,2),'little') for i in range(6)]
spot = rd(16,8).hex()
pb.stop(save=False)
print()
print('%-14s %8s %10s' % ('primitive','frames','GB seconds'))
for lab,v in zip(LABELS, vals):
    print('%-14s %8d %10.2f' % (lab, v, v/59.7))
print()
print('output key starts with %s (expect a60869f0dbcf1dc6)' % spot)
print('finished=%d after %d emulated frames' % (rd(0)[0], f))
