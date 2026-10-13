"""Read the QR code off the emulated Game Boy screen and decode it.

Boots the ROM with an existing wallet in cartridge SRAM, opens RECEIVE, grabs
the framebuffer and hands it to OpenCV's QR reader. Passing means the encoder,
the Reed-Solomon codewords, the mask, the tile generation and the VRAM-bank-1
rendering are all right, because a real reader got the address back.
"""
import os, re, sys
import numpy as np
import cv2

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ROM = os.path.join(ROOT, 'build', 'offwallet.gbc')
NOI = os.path.join(ROOT, 'build', 'offwallet.noi')
syms = {}
for line in open(NOI):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m:
        syms[m.group(1)] = int(m.group(2), 16)
WA = syms['_wa']

if not os.path.exists(ROM + '.ram'):
    sys.exit('no saved wallet: run "python tools/run_game_test.py swift" first '
             '(it saves one to cartridge SRAM)')

from pyboy import PyBoy
from pyboy.utils import WindowEvent

pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)

def tick(n=1):
    for _ in range(n):
        pb.tick()

def tap(k, hold=5, gap=20):
    pb.send_input(k[0]); tick(hold)
    pb.send_input(k[1]); tick(gap)

A = (WindowEvent.PRESS_BUTTON_A, WindowEvent.RELEASE_BUTTON_A)
DOWN = (WindowEvent.PRESS_ARROW_DOWN, WindowEvent.RELEASE_ARROW_DOWN)

def row(y):
    out = ''
    for x in range(20):
        t = pb.get_memory_value(0x9800 + y * 32 + x)
        out += chr(0x20 + t) if t < 95 else '~'
    return out

def rd(addr, n):
    return bytes(pb.get_memory_value(addr + i) for i in range(n))

print('booting with the saved wallet...')
tick(500)
for _ in range(20):                       # clear the "welcome back" dialogue
    if 'SAVE TO CART' in row(15) or 'OFFWALLET' in row(0):
        break
    tap(A, gap=40)
tick(60)
addr = rd(WA + 96, 64).split(bytes([0]))[0].decode('ascii', 'replace')
print('wallet address: %s' % addr)

print('opening RECEIVE...')
tap(DOWN, gap=30)                         # ADDRESS -> RECEIVE
tap(A, gap=60)
# the encoder runs eight mask evaluations; give it time
built = False
for _ in range(120):
    tick(60)
    if 'SCAN TO PAY ME' in row(0):
        built = True
        break
if not built:
    print('the receive screen never appeared; screen row 0: %r' % row(0))
    sys.exit(2)
tick(120)

img = np.array(pb.botsupport_manager().screen().screen_ndarray())
pb.stop(save=False)

gray = cv2.cvtColor(img, cv2.COLOR_RGB2GRAY)
cv2.imwrite(os.path.join(ROOT, 'build', 'shots', 'qr_screen.png'), gray)
# a phone sees a bigger image than 160x144; upscale as a camera would
big = cv2.resize(gray, (gray.shape[1] * 6, gray.shape[0] * 6),
                 interpolation=cv2.INTER_NEAREST)
decoded, _, _ = cv2.QRCodeDetector().detectAndDecode(big)

print()
print('decoded from the Game Boy screen: %r' % decoded)
print('expected (uppercased address):    %r' % addr.upper())
ok = decoded == addr.upper()
print()
print('A QR READER GETS THE ADDRESS OFF THE SCREEN' if ok else 'DECODE MISMATCH')
sys.exit(0 if ok else 1)
