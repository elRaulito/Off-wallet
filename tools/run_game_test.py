"""Drive the real game ROM in an emulator and verify the wallet it produces.

Navigation is screen-driven: the font is loaded so that tile id == ASCII - 0x20,
so the tile map can be read back as text and the harness can tell which scene
is on screen instead of firing button presses blindly.

The check that matters: take the private key the ROM generated, recompute the
taproot address independently in Python, and compare it with what the ROM
computed and with what it printed on screen. A match means the ROM's crypto is
right for a key nobody chose in advance.

usage: python tools/run_game_test.py [swift|ancient]
"""
import os, re, sys, hashlib, hmac as hmaclib, io

MODE = sys.argv[1] if len(sys.argv) > 1 else 'swift'
ROM = 'build/offwallet.gbc'
NOI = 'build/offwallet.noi'
SHOTS = 'build/shots'
HATCH_BUDGET = int(os.environ.get('HATCH_BUDGET', '1500000'))

os.makedirs(SHOTS, exist_ok=True)
# Start from a blank cartridge: a leftover save makes the ROM skip straight to
# the wallet screen (which is itself proof that the battery save works).
for ext in ('.ram', '.sav'):
    stale = ROM + ext
    if os.path.exists(stale):
        os.remove(stale)
        print('removed stale save %s' % stale)
syms = {}
for line in open(NOI):
    m = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line.strip())
    if m:
        syms[m.group(1)] = int(m.group(2), 16)
WA, KIND, WORDS = syms['_wa'], syms['_kind'], syms['_words']
PROGRESS = syms['_ow_progress']

from pyboy import PyBoy
from pyboy.utils import WindowEvent

pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)

PRESS = {
    'a':     (WindowEvent.PRESS_BUTTON_A,     WindowEvent.RELEASE_BUTTON_A),
    'b':     (WindowEvent.PRESS_BUTTON_B,     WindowEvent.RELEASE_BUTTON_B),
    'start': (WindowEvent.PRESS_BUTTON_START, WindowEvent.RELEASE_BUTTON_START),
    'left':  (WindowEvent.PRESS_ARROW_LEFT,   WindowEvent.RELEASE_ARROW_LEFT),
    'right': (WindowEvent.PRESS_ARROW_RIGHT,  WindowEvent.RELEASE_ARROW_RIGHT),
    'up':    (WindowEvent.PRESS_ARROW_UP,     WindowEvent.RELEASE_ARROW_UP),
    'down':  (WindowEvent.PRESS_ARROW_DOWN,   WindowEvent.RELEASE_ARROW_DOWN),
}
frames = [0]

def tick(n=1):
    for _ in range(n):
        pb.tick()
        frames[0] += 1

def tap(key, hold=5, gap=14):
    p, r = PRESS[key]
    pb.send_input(p);  tick(hold)
    pb.send_input(r);  tick(gap)

def shot(name):
    pb.botsupport_manager().screen().screen_image().resize((320, 288)) \
      .save(os.path.join(SHOTS, name + '.png'))

def rd(addr, n):
    return bytes(pb.get_memory_value(addr + i) for i in range(n))

def screen():
    rows = []
    for y in range(18):
        row = ''
        for x in range(20):
            t = pb.get_memory_value(0x9800 + y * 32 + x)
            row += chr(0x20 + t) if t < 95 else '~'
        rows.append(row.rstrip())
    return rows

def has(text, rows=None):
    return any(text in r for r in (rows if rows is not None else screen()))

def row(y):
    """One row of the tile map as text -- cheap enough to poll every frame."""
    out = ''
    for x in range(20):
        t = pb.get_memory_value(0x9800 + y * 32 + x)
        out += chr(0x20 + t) if t < 95 else '~'
    return out

def banner():
    """The scene titles sit in the top rows; rows 13-15 catch menu entries and
    prompts wherever the content file puts them."""
    return '|'.join(row(y) for y in (0, 1, 2, 13, 14, 15, 16))

def settle(max_frames=400, stable_for=18):
    """Run frames until the screen stops changing: when the game is waiting for
    input it is idle, so a stable banner means a scene is actually on display.
    Polling only between blind presses misses scenes that draw and confirm in
    the same frame while a button is held."""
    prev, stable = None, 0
    for _ in range(max_frames):
        tick(1)
        cur = banner()
        if cur == prev:
            stable += 1
            if stable >= stable_for:
                return cur
        else:
            prev, stable = cur, 0
    return banner()

def advance_until(text, key='a', tries=80, label=''):
    for _ in range(tries):
        b = settle()
        if text in b:
            return
        tap(key)
    print('--- stuck; screen now: ---')
    for r_ in screen():
        print('  |%s|' % r_)
    raise SystemExit('never reached %r %s' % (text, label))

print('booting...')
tick(400)          # let the Game Boy boot logo finish; it is stable too
settle()
shot('01_title')
if 'PRESS START' not in banner():
    print('banner: %r' % banner())
    for r_ in screen():
        print('  |%s|' % r_)
    raise SystemExit('title screen missing')

tap('start', gap=40)
print('walking the professor dialogue...')
advance_until('CHOOSE A STARTER', label='(starter screen)')
shot('02_starter')

print('choosing BITCOIN...')
advance_until('PICK YOUR EGG', label='(egg selection)')
shot('03_egg_kind')

if MODE == 'ancient':
    for _ in range(10):
        if has('> 12 words'):
            break
        tap('right', gap=25)
    assert has('> 12 words'), 'could not select the ANCIENT egg'
else:
    assert has('> one key'), 'SWIFT egg not selected by default'
shot('04_egg_selected')

print('confirming the egg...')
advance_until('FLIP YOUR OWN KEY', label='(coin flip screen)')
shot('05_flip_screen')

print('flipping coins...')
keys = ['a', 'b', 'up', 'down', 'left', 'right']
for i in range(600):
    if has('START: keep these'):
        break
    tap(keys[i % len(keys)], hold=3, gap=6)
assert has('START: keep these'), 'the coin flip screen never filled up'
shot('06_flips_done')

# The bits on that screen are the whole secret, so read them off the tile map
# exactly as a person would read them off the console, and check further down
# that they really are what the key was built from. Where the grid sits comes
# from [config], so moving it in the content file cannot break this.
CONTENT = io.open('content/story.txt', encoding='utf-8').read()


def cfg(name):
    m = re.search(r'^\s*%s\s*=\s*(\S+)' % re.escape(name), CONTENT, re.M)
    if not m:
        raise SystemExit('content/story.txt has no [config] %s' % name)
    return m.group(1)


def tile_ch(x, y):
    t = pb.get_memory_value(0x9800 + y * 32 + x)
    return chr(0x20 + t) if t < 95 else '~'


FX, FY = (int(v) for v in cfg('flip_grid').split(','))
FCOLS = int(cfg('flip_grid_cols'))
NFLIPS = 128
flips = ''
for i in range(NFLIPS):
    ch = tile_ch(FX + i % FCOLS, FY + i // FCOLS)
    if ch not in 'HT':
        raise SystemExit('flip %d on screen is %r, not H or T' % (i, ch))
    flips += ch
flip_bits = ''.join('1' if c == 'H' else '0' for c in flips)
flip_entropy = int(flip_bits, 2).to_bytes(NFLIPS // 8, 'big')
print('  flips on screen  %s' % flips[:64])
print('                   %s' % flips[64:])
print('  heads %d  tails %d  (a scripted emulator is not a fair coin; this is'
      % (flips.count('H'), flips.count('T')))
print('   why the key must be flipped on real hardware)')

print('starting the hatch...')
advance_until('HATCHING', label='(hatching screen)')
shot('07_hatching')

kind = pb.get_memory_value(KIND)
print('mode in ROM: kind=%d (%s)' % (kind, 'BIP-39' if kind else 'single key'))

start = frames[0]
last = -1
while frames[0] - start < HATCH_BUDGET:
    tick(600)
    p = pb.get_memory_value(PROGRESS)
    if p != last:
        last = p
        print('  progress %d at %.0fs of GB time' % (p, (frames[0] - start) / 59.7))
    a = rd(WA + 96, 64).split(bytes([0]))[0]
    if a.startswith(b'bc1p') and len(a) == 62:
        break
hatch_frames = frames[0] - start
shot('08_hatched')

priv = rd(WA, 32)
internal = rd(WA + 32, 32)
output = rd(WA + 64, 32)
addr = rd(WA + 96, 64).split(bytes([0]))[0].decode('ascii', 'replace')
word_idx = [int.from_bytes(rd(WORDS + i * 2, 2), 'little') for i in range(12)]

print('reaching the wallet menu...')
advance_until('SAVE TO CART', label='(wallet menu)')
shot('09_wallet_menu')

def open_entry(menu_row, name):
    """Walk the wallet menu cursor to `menu_row` and open that entry. Driven by
    where the '>' actually is, so adding menu entries cannot break this."""
    for _ in range(12):
        if row(menu_row).startswith('>'):
            break
        tap('down', gap=25)
    assert row(menu_row).startswith('>'),         'cursor never reached row %d for %s' % (menu_row, name)
    tap('a', gap=60)
    got = screen()
    shot(name)
    tap('b', gap=30)
    return got

# menu rows come from [config] in the content file: first entry at 6, step 2
screens = {}
screens['address'] = open_entry(6, '10_address')
screens['secret'] = open_entry(12, '11_secret')
screens['output'] = open_entry(14, '12_output')

# SAVE TO CART, then reboot and confirm the wallet comes back byte-identical
print('saving to cartridge SRAM...')
tap('down', gap=25)
tap('a', gap=60)
for _ in range(8):
    tap('a', gap=30)
shot('13_saved')
pb.stop(save=True)

pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
tick(600)
reloaded_addr = rd(WA + 96, 64).split(bytes([0]))[0].decode('ascii', 'replace')
reloaded_priv = rd(WA, 32)
shot('14_after_reboot')
reboot_screen = screen()
pb.stop(save=False)

print()
print('mode           %s (kind=%d)' % (MODE, kind))
print('hatch time     %d frames = %.0fs of Game Boy time'
      % (hatch_frames, hatch_frames / 59.7))
print('private key    %s' % priv.hex())
print('output key     %s' % output.hex())
print('address        %s' % addr)

# ---------------- independent verification ----------------
P = 2**256 - 2**32 - 977
N = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
GX = 0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
GY = 0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8

def inv(a, m=P): return pow(a, m - 2, m)
def padd(A, B):
    if A is None: return B
    if B is None: return A
    (x1, y1), (x2, y2) = A, B
    if x1 == x2:
        if (y1 + y2) % P == 0: return None
        l = 3 * x1 * x1 * inv(2 * y1) % P
    else:
        l = (y2 - y1) * inv(x2 - x1) % P
    x3 = (l * l - x1 - x2) % P
    return (x3, (l * (x1 - x3) - y1) % P)
def pmul(k, Pt):
    R = None
    while k:
        if k & 1: R = padd(R, Pt)
        Pt = padd(Pt, Pt); k >>= 1
    return R
def tagged(tag, msg):
    t = hashlib.sha256(tag.encode()).digest()
    return hashlib.sha256(t + t + msg).digest()

CHARSET = 'qpzry9x8gf2tvdw0s3jn54khce6mua7l'
def bech32m(hrp, witver, prog):
    GEN = [0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3]
    def polymod(v):
        chk = 1
        for x in v:
            b = chk >> 25
            chk = ((chk & 0x1ffffff) << 5) ^ x
            for i in range(5):
                if (b >> i) & 1: chk ^= GEN[i]
        return chk
    data = [witver]
    acc = bits = 0
    for b in prog:
        acc = (acc << 8) | b; bits += 8
        while bits >= 5:
            bits -= 5; data.append((acc >> bits) & 31)
    if bits: data.append((acc << (5 - bits)) & 31)
    exp = [ord(c) >> 5 for c in hrp] + [0] + [ord(c) & 31 for c in hrp]
    chk_ = polymod(exp + data + [0] * 6) ^ (1 if witver == 0 else 0x2bc830a3)
    return hrp + '1' + ''.join(CHARSET[d] for d in data) + \
           ''.join(CHARSET[(chk_ >> 5 * (5 - i)) & 31] for i in range(6))

fails = 0
def chk(name, got, exp):
    global fails
    ok = got == exp
    print('%-26s %s' % (name, 'ok' if ok else 'MISMATCH'))
    if not ok:
        fails += 1
        print('  rom    %s' % got)
        print('  python %s' % exp)

print()
WORDLIST = [w.strip() for w in
            io.open('test/vectors/bip39-english.txt', encoding='utf-8').read().split()]

if kind == 1:
    mnemonic = ' '.join(WORDLIST[i] for i in word_idx)
    print('mnemonic       %s' % mnemonic)
    seed = hashlib.pbkdf2_hmac('sha512', mnemonic.encode(), b'mnemonic', 2048, 64)

    def ckd(kpar, cpar, index):
        if index >= 0x80000000:
            data = bytes([0]) + kpar.to_bytes(32, 'big') + index.to_bytes(4, 'big')
        else:
            Pt = pmul(kpar, (GX, GY))
            data = bytes([3 if Pt[1] & 1 else 2]) + Pt[0].to_bytes(32, 'big') \
                   + index.to_bytes(4, 'big')
        I = hmaclib.new(cpar, data, hashlib.sha512).digest()
        return (int.from_bytes(I[:32], 'big') + kpar) % N, I[32:]

    I = hmaclib.new(b'Bitcoin seed', seed, hashlib.sha512).digest()
    kk, cc = int.from_bytes(I[:32], 'big'), I[32:]
    for idx in (86 | 0x80000000, 0x80000000, 0x80000000, 0, 0):
        kk, cc = ckd(kk, cc, idx)
    print()
    chk('BIP-39 -> m/86h/0h/0h/0/0', priv.hex(), kk.to_bytes(32, 'big').hex())

k = int.from_bytes(priv, 'big')
if not (0 < k < N):
    print('private key out of range!')
    fails += 1
else:
    Pt = pmul(k, (GX, GY))
    lift = (Pt[0], Pt[1] if Pt[1] % 2 == 0 else (-Pt[1]) % P)
    t = int.from_bytes(tagged('TapTweak', lift[0].to_bytes(32, 'big')), 'big')
    Q = padd(lift, pmul(t, (GX, GY)))
    exp_output = Q[0].to_bytes(32, 'big')
    exp_addr = bech32m('bc', 1, exp_output)

    chk('internal key', internal.hex(), Pt[0].to_bytes(32, 'big').hex())
    chk('output key', output.hex(), exp_output.hex())
    chk('address', addr, exp_addr)

    def joined(rows, lo, hi):
        return ''.join(r.replace(' ', '') for r in rows[lo:hi])

    chk('address on screen', joined(screens['address'], 2, 8), exp_addr)
    chk('output key on screen', joined(screens['output'], 2, 6), exp_output.hex())

    # What the player watched land must be exactly what the key came from.
    if kind == 0:
        chk('key = sha256(the flips)', priv.hex(),
            hashlib.sha256(flip_entropy).hexdigest())
    else:
        ent_bits = ''.join(format(b, '08b') for b in flip_entropy)
        chkbits = format(hashlib.sha256(flip_entropy).digest()[0], '08b')[:4]
        idx = [int((ent_bits + chkbits)[i * 11:(i + 1) * 11], 2)
               for i in range(12)]
        chk('the flips are the BIP-39 entropy',
            ' '.join(WORDLIST[i] for i in idx),
            ' '.join(WORDLIST[i] for i in word_idx))

    if kind == 0:
        payload = bytes([0x80]) + priv + bytes([0x01])
        payload += hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
        n = int.from_bytes(payload, 'big')
        A58 = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
        wif = ''
        while n:
            n, r = divmod(n, 58)
            wif = A58[r] + wif
        chk('WIF on screen', joined(screens['secret'], 4, 9), wif)
        chk('hex key on screen', joined(screens['secret'], 10, 14), priv.hex())
    else:
        # one word per row, numbered "n. word"
        got = []
        for r in screens['secret'][2:14]:
            parts = r.split()
            if len(parts) == 2 and parts[0].endswith('.'):
                got.append(parts[1])
        chk('12 words on screen', ' '.join(got),
            ' '.join(WORDLIST[i] for i in word_idx))

chk('wallet survives reboot', reloaded_addr, addr)
chk('key survives reboot', reloaded_priv.hex(), priv.hex())
if not any('Welcome back' in r or 'OFFWALLET' in r for r in reboot_screen):
    print('note: after reboot the screen showed:')
    for r in reboot_screen[:6]:
        print('  |%s|' % r)

print()
print('THE ROM PRODUCED A CORRECT TAPROOT WALLET' if not fails
      else '%d MISMATCHES' % fails)
sys.exit(1 if fails else 0)
