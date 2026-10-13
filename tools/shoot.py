"""Capture the screenshots used in README.md.

Two passes. The first starts from a blank cartridge and walks the opening:
title, professor, starter, egg, coin flips, hatch. The second boots a cartridge
that already holds a wallet and a UTXO, which is the only way to photograph the
wallet menu with a balance on it.

Navigation is screen-driven, like the tests: the font is loaded so tile id ==
ASCII - 0x20, so the tile map reads back as text and the script can tell which
scene is up instead of firing presses blindly.

usage: python tools/shoot.py
"""
import os
import shutil

from PIL import Image
from pyboy import PyBoy
from pyboy.utils import WindowEvent

ROM = 'build/offwallet.gbc'
SAVE = ROM + '.ram'
WITH_UTXO = 'build/with_utxo.ram'
OUT = 'docs/shots'
SCALE = 3

os.makedirs(OUT, exist_ok=True)

PRESS = {
    'a':      (WindowEvent.PRESS_BUTTON_A,      WindowEvent.RELEASE_BUTTON_A),
    'b':      (WindowEvent.PRESS_BUTTON_B,      WindowEvent.RELEASE_BUTTON_B),
    'start':  (WindowEvent.PRESS_BUTTON_START,  WindowEvent.RELEASE_BUTTON_START),
    'select': (WindowEvent.PRESS_BUTTON_SELECT, WindowEvent.RELEASE_BUTTON_SELECT),
    'left':   (WindowEvent.PRESS_ARROW_LEFT,    WindowEvent.RELEASE_ARROW_LEFT),
    'right':  (WindowEvent.PRESS_ARROW_RIGHT,   WindowEvent.RELEASE_ARROW_RIGHT),
    'up':     (WindowEvent.PRESS_ARROW_UP,      WindowEvent.RELEASE_ARROW_UP),
    'down':   (WindowEvent.PRESS_ARROW_DOWN,    WindowEvent.RELEASE_ARROW_DOWN),
}

pb = None


def tick(n=1):
    for _ in range(n):
        pb.tick()


def tap(key, hold=5, gap=14):
    p, r = PRESS[key]
    pb.send_input(p)
    tick(hold)
    pb.send_input(r)
    tick(gap)


def row(y):
    out = ''
    for x in range(20):
        t = pb.get_memory_value(0x9800 + y * 32 + x)
        out += chr(0x20 + t) if t < 95 else '~'
    return out


def banner():
    """The whole tile map. The tests sample a few rows for speed, but a
    screenshot has to wait for the last thing drawn, and some screens fill
    their middle rows a good few frames after the headings land."""
    return '|'.join(row(y) for y in range(18))


def has(text):
    return any(text in row(y) for y in range(18))


def settle(max_frames=900, stable_for=45):
    """Wait until the tile map stops changing. 45 frames, because the wallet
    menu checksums the UTXO store before it can print a balance and that takes
    the better part of half a second on this CPU."""
    prev, stable = None, 0
    for _ in range(max_frames):
        tick(1)
        cur = banner()
        if cur == prev:
            stable += 1
            if stable >= stable_for:
                return
        else:
            prev, stable = cur, 0


def shot(name):
    img = pb.botsupport_manager().screen().screen_image().convert('RGB')
    img = img.resize((160 * SCALE, 144 * SCALE), Image.NEAREST)
    path = os.path.join(OUT, name + '.png')
    img.save(path)
    print('  %s' % path)


def advance_until(text, key='a', tries=80):
    for _ in range(tries):
        settle()
        if has(text):
            return
        tap(key)
    raise SystemExit('never reached %r' % text)


def boot(save=None):
    global pb
    for stale in (SAVE, ROM + '.sav'):
        if os.path.exists(stale):
            os.remove(stale)
    if save:
        shutil.copyfile(save, SAVE)
    pb = PyBoy(ROM, window_type='headless', debug=False)
    pb.set_emulation_speed(0)
    tick(400)
    settle()


# ---------------------------------------------------- pass 1: a blank cart
print('blank cartridge:')
boot()
shot('01-title')

tap('start', gap=40)
settle()
shot('02-professor')

advance_until('CHOOSE A STARTER')
shot('03-starter')

advance_until('PICK YOUR EGG')
shot('04-egg')

advance_until('FLIP YOUR OWN KEY')
keys = ['a', 'b', 'up', 'down', 'left', 'right']
for i in range(600):                      # stop part way: a half-full grid
    if has('flips 058'):
        break
    tap(keys[i % len(keys)], hold=3, gap=6)
settle()
shot('05-flipping')

for i in range(600):
    if has('START: keep these'):
        break
    tap(keys[i % len(keys)], hold=3, gap=6)
settle()
shot('06-flips-done')

advance_until('HATCHING')
tick(240)
shot('07-hatching')
pb.stop(save=False)

# ------------------------------------------- pass 2: a cart with a wallet
print('cartridge holding a wallet and a UTXO:')
boot(WITH_UTXO)
shot('08-welcome-back')

advance_until('SAVE TO CART')
shot('09-wallet-menu')


def open_entry(menu_row, name, wait_for=None):
    for _ in range(12):
        if row(menu_row).startswith('>'):
            break
        tap('down', gap=25)
    tap('a', gap=60)
    if wait_for:
        # The QR encoder evaluates eight masks before anything is drawn, and
        # it blanks the screen while it fills VRAM, so wait for the finished
        # caption rather than for the picture to stop changing.
        for _ in range(200):
            tick(60)
            if wait_for in row(0):
                break
        else:
            raise SystemExit('%s never appeared' % wait_for)
        tick(120)
    settle()
    shot(name)
    tap('b', gap=30)
    settle()


open_entry(6, '10-address')
open_entry(8, '11-receive-qr', wait_for='SCAN TO PAY ME')
open_entry(10, '12-utxos')
open_entry(12, '13-secret')
pb.stop(save=False)

print('done')
