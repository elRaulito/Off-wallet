"""Check the SM83 assembly multiply against the C one, inside the emulator."""
import re, sys, os
ROM='build/testmul.gbc'; NOI='build/testmul.noi'
syms={}
for line in open(NOI):
    m=re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)',line.strip())
    if m: syms[m.group(1)]=int(m.group(2),16)
B=syms['_tm_out']
from pyboy import PyBoy
pb=PyBoy(ROM,window_type='headless',debug=False); pb.set_emulation_speed(0)
rd=lambda o,n=1: bytes(pb.get_memory_value(B+o+i) for i in range(n))
f=0
while f < 400000:
    pb.tick(); f+=1
    if rd(0)[0]==1: break
rounds=rd(1)[0]; bad=rd(2)[0]
t_asm=int.from_bytes(rd(3,2),'little'); t_c=int.from_bytes(rd(5,2),'little')
a_lo=rd(7,32).hex(); c_lo=rd(39,32).hex()
pb.stop(save=False)
print('rounds completed   %d' % rounds)
print('mismatches         %d' % bad)
print('32 asm multiplies  %d frames (%.1f ms each)' % (t_asm, t_asm/59.7/32*1000))
print('32 C   multiplies  %d frames (%.1f ms each)' % (t_c, t_c/59.7/32*1000))
if bad:
    print('first mismatch, low 32 bytes:')
    print('  asm %s' % a_lo)
    print('  c   %s' % c_lo)
print()
print('ASSEMBLY MULTIPLY MATCHES THE C REFERENCE' if rounds==32 and not bad
      else 'ASSEMBLY MULTIPLY IS WRONG')
sys.exit(0 if (rounds==32 and not bad) else 1)
