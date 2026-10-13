"""Check that content/story.txt is what the ROM actually shows.

Parses the text and sprite commands of a few screens out of the content file,
boots the ROM, and compares them against the emulated tile map character by
character at the exact coordinates. If someone edits the content file and the
screen does not change, this fails.

usage: python tools/run_content_test.py
"""
import io, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
STORY = os.path.join(ROOT, 'content', 'story.txt')
ROM = os.path.join(ROOT, 'build', 'offwallet.gbc')

# Screens we can reach without generating a wallet.
CHECK = ['title', 'starter', 'egg_swift', 'egg_ancient']


def parse_screens(path):
    screens, cur = {}, None
    for raw in io.open(path, encoding='utf-8'):
        line = raw.rstrip('\n')
        s = line.strip()
        if not s or s.startswith('#'):
            continue
        m = re.match(r'^\[screen\s+([\w.]+)\]$', s)
        if m:
            cur = m.group(1)
            screens[cur] = []
            continue
        if re.match(r'^\[', s):
            cur = None
            continue
        if cur is None:
            continue
        parts = s.split(None, 2)
        if parts[0] in ('text', 'accent', 'warn') and len(parts) >= 3:
            x, y = (int(v) for v in parts[1].split(','))
            after = line.rstrip()
            after = after[after.index(parts[1]) + len(parts[1]):]
            screens[cur].append((x, y, after[1:] if after.startswith(' ') else after))
    return screens


screens = parse_screens(STORY)
missing = [s for s in CHECK if s not in screens]
if missing:
    sys.exit('content file has no screen(s): %s' % ', '.join(missing))

from pyboy import PyBoy
from pyboy.utils import WindowEvent

for stale in (ROM + '.ram', ROM + '.sav'):
    if os.path.exists(stale):
        os.remove(stale)

pb = PyBoy(ROM, window_type='headless', debug=False)
pb.set_emulation_speed(0)
A = (WindowEvent.PRESS_BUTTON_A, WindowEvent.RELEASE_BUTTON_A)
ST = (WindowEvent.PRESS_BUTTON_START, WindowEvent.RELEASE_BUTTON_START)
RIGHT = (WindowEvent.PRESS_ARROW_RIGHT, WindowEvent.RELEASE_ARROW_RIGHT)
LEFT = (WindowEvent.PRESS_ARROW_LEFT, WindowEvent.RELEASE_ARROW_LEFT)


def tick(n=1):
    for _ in range(n):
        pb.tick()


def tap(k, hold=5, gap=14):
    pb.send_input(k[0]); tick(hold)
    pb.send_input(k[1]); tick(gap)


def row(y):
    out = ''
    for x in range(20):
        t = pb.get_memory_value(0x9800 + y * 32 + x)
        out += chr(0x20 + t) if t < 95 else '~'
    return out


def settle(max_frames=400, stable_for=18):
    prev, stable = None, 0
    for _ in range(max_frames):
        tick(1)
        cur = row(0) + row(1) + row(2) + row(13) + row(14)
        if cur == prev:
            stable += 1
            if stable >= stable_for:
                return
        else:
            prev, stable = cur, 0


fails = []


def verify(name):
    """Every text command of `name` must be on screen where it says."""
    bad = 0
    for (x, y, text) in screens[name]:
        got = row(y)[x:x + len(text)]
        if got != text:
            bad += 1
            fails.append('screen %-12s row %2d col %2d\n    content file: %r\n'
                         '    on screen:    %r' % (name, y, x, text, got))
    print('  %-12s %2d text commands, %d wrong'
          % (name, len(screens[name]), bad))


print('checking that the ROM shows what content/story.txt says')
tick(400)
settle()
verify('title')

tap(ST, gap=40)
for _ in range(80):                       # walk the professor's dialogue
    settle()
    if 'CHOOSE A STARTER' in row(1):
        break
    tap(A)
verify('starter')

# The two locked starters must refuse to be chosen. Timing matters here: the
# dialogue box crawls its text out and then closes on ANY key, so each press
# has to wait for the crawl to settle or it gets swallowed by the box.
print('checking that B and C cannot be picked...')
for slot in (1, 2):
    settle()                              # let the dialogue finish crawling
    tap(A, gap=25)                        # close the box
    tap(RIGHT, gap=30)                    # move onto the locked egg
    settle()
    tap(A, gap=25)                        # close its "never hatched" box
    tap(A, gap=30)                        # try to choose it
    settle()
    if 'PICK YOUR EGG' in row(1):
        sys.exit('slot %d was selectable, but should be locked' % slot)
    if 'CHOOSE A STARTER' not in row(1):
        sys.exit('starter screen vanished while on locked slot %d' % slot)
print('  both locked slots refused, as they should')
for _ in range(2):                        # walk back to BITCOIN
    settle()
    tap(A, gap=25)
    tap(LEFT, gap=30)

for _ in range(40):                       # confirm BITCOIN, reach the eggs
    settle()
    if 'PICK YOUR EGG' in row(1):
        break
    tap(A)
verify('egg_swift')

tap(RIGHT, gap=30)
settle()
verify('egg_ancient')

pb.stop(save=False)

print()
if fails:
    print('MISMATCHES:')
    for f in fails:
        print('  ' + f)
    print('\n%d wrong' % len(fails))
    sys.exit(1)
print('THE ROM MATCHES THE CONTENT FILE')
