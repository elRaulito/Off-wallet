"""Type a UTXO into the wallet through the emulator and check it sticks.

Drives the entry widget the way a player would (pick a hex digit, press A, 64
times), then reads the stored table out of WRAM, checks the balance shown on
screen, reboots, and checks the entry survived in cartridge SRAM.
"""
import os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ROM = os.path.join(ROOT, 'build', 'offwallet.gbc')
syms = {}
for line in open(os.path.join(ROOT, 'build', 'offwallet.noi')):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m:
        syms[m.group(1)] = int(m.group(2), 16)
TAB, CNT = syms['_utxo_tab'], syms['_utxo_count']

if not os.path.exists(ROM + '.ram'):
    sys.exit('run "python tools/run_game_test.py swift" first to make a wallet')

from pyboy import PyBoy
from pyboy.utils import WindowEvent

pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
K = {
    'a': (WindowEvent.PRESS_BUTTON_A, WindowEvent.RELEASE_BUTTON_A),
    'b': (WindowEvent.PRESS_BUTTON_B, WindowEvent.RELEASE_BUTTON_B),
    'start': (WindowEvent.PRESS_BUTTON_START, WindowEvent.RELEASE_BUTTON_START),
    'select': (WindowEvent.PRESS_BUTTON_SELECT, WindowEvent.RELEASE_BUTTON_SELECT),
    'left': (WindowEvent.PRESS_ARROW_LEFT, WindowEvent.RELEASE_ARROW_LEFT),
    'right': (WindowEvent.PRESS_ARROW_RIGHT, WindowEvent.RELEASE_ARROW_RIGHT),
    'up': (WindowEvent.PRESS_ARROW_UP, WindowEvent.RELEASE_ARROW_UP),
    'down': (WindowEvent.PRESS_ARROW_DOWN, WindowEvent.RELEASE_ARROW_DOWN),
}

def tick(n=1):
    for _ in range(n):
        pb.tick()

def tap(name, hold=4, gap=8):
    p, r = K[name]
    pb.send_input(p); tick(hold)
    pb.send_input(r); tick(gap)

def row(y):
    out = ''
    for x in range(20):
        t = pb.get_memory_value(0x9800 + y * 32 + x)
        out += chr(0x20 + t) if t < 95 else '~'
    return out

def rd(a, n):
    return bytes(pb.get_memory_value(a + i) for i in range(n))

HEX = '0123456789abcdef'

def type_chars(chars):
    """The picker starts on '0'; walk right to each character and press A."""
    pos = 0
    for c in chars:
        want = HEX.index(c)
        # the picker wraps, so go whichever way is shorter
        n = len(HEX)
        fwd = (want - pos) % n
        back = (pos - want) % n
        if fwd <= back:
            for _ in range(fwd): tap('right', gap=3)
        else:
            for _ in range(back): tap('left', gap=3)
        pos = want
        tap('a', gap=3)

TXID = 'ab' * 32                     # 64 hex characters
VOUT = '001'
SATS = '0000123456'                  # 123456 satoshis

print('booting...')
tick(500)
for _ in range(20):
    if 'OFFWALLET' in row(0):
        break
    tap('a', gap=40)

print('opening MY UTXOS...')
# the menu cursor is the '>' in column 0; MY UTXOS is the third entry, row 10
for _ in range(12):
    if row(10).startswith('>'):
        break
    tap('down', gap=25)
assert row(10).startswith('>'), 'cursor never reached MY UTXOS'
tap('a', gap=60)
assert 'MY UTXOS' in row(0), 'never reached the UTXO screen: row0=%r' % row(0)

print('adding an entry (this types 64 hex digits)...')
tap('a', gap=30)
assert 'TXID' in row(0), 'no txid screen: %r' % row(0)
type_chars(TXID)
tap('start', gap=30)
assert 'OUTPUT INDEX' in row(0), 'no vout screen: %r' % row(0)
type_chars(VOUT)
tap('start', gap=30)
assert 'AMOUNT IN SATS' in row(0), 'no value screen: %r' % row(0)
type_chars(SATS)
tap('start', gap=60)

count = pb.get_memory_value(CNT)
entry = rd(TAB, 38)
balance_row = row(2)
print('entries: %d' % count)
print('txid in RAM: %s' % entry[:8].hex())
print('balance row: %r' % balance_row.strip())

pb.stop(save=True)

# reboot and check it came back from SRAM
pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
tick(500)
for _ in range(20):
    if 'OFFWALLET' in row(0):
        break
    tap('a', gap=40)
tick(60)
after_count = pb.get_memory_value(CNT)
after_entry = rd(TAB, 38)
after_balance = row(4)
pb.stop(save=False)

fails = 0
def chk(name, got, exp):
    global fails
    ok = got == exp
    print('%-26s %s' % (name, 'ok' if ok else 'FAIL'))
    if not ok:
        fails += 1
        print('   got %r\n   exp %r' % (got, exp))

chk('entry count', count, 1)
chk('txid stored', entry[:32].hex(), TXID)
chk('vout stored', entry[32], int(VOUT))
chk('value stored', int.from_bytes(entry[33:37], 'little'), int(SATS))
chk('balance on screen', '0.00123456' in balance_row, True)
chk('survives reboot', after_entry[:32].hex(), TXID)
chk('count after reboot', after_count, 1)
chk('balance after reboot', '0.00123456' in after_balance, True)
print()
print('THE UTXO LIST WORKS AND PERSISTS' if not fails else '%d FAILURES' % fails)
sys.exit(1 if fails else 0)
