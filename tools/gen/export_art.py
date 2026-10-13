"""One-off: export the sprites that used to be ASCII art into editable PNGs.

After this, content/art/*.png is the source of truth for the artwork and can be
drawn in any image editor. Kept in the repo so the conversion is reproducible
and so it is obvious which four colours each file is allowed to use.
"""
import io, os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
OUT = os.path.join(ROOT, 'content', 'art')
os.makedirs(OUT, exist_ok=True)

# The palettes these sprites are drawn with, as they are defined in ui.c
# (RGB(31,...) values scaled back up to 8 bits per channel).
PAL = {
    # BTC egg: gold shell, dark line art. Ordered light -> dark, because the
    # generator assigns palette index 0 to the lightest colour (the background).
    'egg_btc':    [(248, 248, 224), (255, 240, 190), (255, 176, 32), (150, 90, 0)],
    # the two locked eggs use identical colours, so they share one palette slot
    'egg_locked': [(248, 248, 224), (232, 232, 232), (170, 170, 180), (80, 80, 90)],
    'ball': [(255, 255, 255), (255, 82, 82), (49, 33, 49), (165, 33, 33)],
    'btc':  [(255, 255, 255), (255, 181, 49), (66, 41, 16), (214, 123, 24)],
    'egg':  [(255, 255, 255), (231, 214, 165), (99, 82, 66), (181, 156, 115)],
    'prof': [(248, 248, 224), (232, 232, 240), (240, 192, 144), (56, 32, 24)],
}

# An egg shell with a letter on it. '+' is the shell, '#' the outline and the
# glyph, '-' a highlight. The BTC one carries the two ticks that make a B into
# a bitcoin sign.
def egg(glyph, ticks):
    body = [
        '.....######.....',
        '...##++++++##...',
        '..#++++++++++#..',
        '.#++++++++++++#.',
        '.#++++++++++++#.',
        '#++++++++++++++#',
        '#++++++++++++++#',
        '#++++++++++++++#',
        '#++++++++++++++#',
        '#++++++++++++++#',
        '#++++++++++++++#',
        '.#++++++++++++#.',
        '.#++++++++++++#.',
        '..#++++++++++#..',
        '...##++++++##...',
        '.....######.....',
    ]
    rows = [list(r) for r in body]
    # the glyph is five columns wide, seven rows tall, at columns 5..9
    for gy, line in enumerate(glyph):
        for gx, ch in enumerate(line):
            if ch == '#':
                rows[4 + gy][5 + gx] = '#'
    if ticks:
        for y in (3, 11):
            rows[y][6] = '#'
            rows[y][8] = '#'
    return [''.join(r) for r in rows]

GLYPH_B = ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.']
GLYPH_C = ['.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.']

LEGEND = {'.': 0, '-': 1, '+': 2, '#': 3}

ART = {}

ART['ball'] = ('ball', [
    '....++++++++....',
    '..++############',
    '.+##############',
    '.+##############',
    '+###############',
    '+###############',
    '+###############',
    '+###############',
    '++++++++++++++++',
    '..++++++++++++..',
    '.+......+++++++.',
    '.+..--++++--..+.',
    '.+..--++++--..+.',
    '.+......+++++++.',
    '..++++++++++++..',
    '....++++++++....',
])
# the original right-hand tiles were mirrored; rebuild them symmetrically
ART['ball'] = ('ball', [
    '....++++++++....',
    '..++########++..',
    '.+##########+#+.',
    '.+############+.',
    '+##############+',
    '+##############+',
    '+##############+',
    '+##############+',
    '++++++++++++++++',
    '..++++++++++++..',
    '.+............+.',
    '.+..--++++--..+.',
    '.+..--++++--..+.',
    '.+............+.',
    '..++++++++++++..',
    '....++++++++....',
])
ART['egg'] = ('egg', [
    '.....++++++.....',
    '...++######++...',
    '..+##########+..',
    '.+############+.',
    '.+############+.',
    '+##############+',
    '+##############+',
    '+##############+',
    '+##############+',
    '+##############+',
    '+##############+',
    '.+############+.',
    '.+############+.',
    '..+##########+..',
    '...++######++...',
    '.....++++++.....',
])
ART['btc'] = ('btc', [
    '....+.....+.....',
    '...++.....++....',
    '..+###...###+...',
    '.+#####.#####+..',
    '+###..###...##+.',
    '+##.......###+..',
    '+##.......###+..',
    '+##..#########+.',
    '+##..#########+.',
    '+##.......###+..',
    '+##.......###+..',
    '+###..###...##+.',
    '.+#####.#####+..',
    '..+###...###+...',
    '...++.....++....',
    '....+.....+.....',
])
ART['egg_btc'] = ('egg_btc', egg(GLYPH_B, ticks=True))
ART['egg_b']   = ('egg_locked', egg(GLYPH_B, ticks=False))
ART['egg_c']   = ('egg_locked', egg(GLYPH_C, ticks=False))

ART['prof'] = ('prof', [
    '................',
    '....########....',
    '..############..',
    '.##############.',
    '.###++++++++###.',
    '.##++++++++++##.',
    '.#++++++++++++#.',
    '.#++##++++##++#.',
    '.#++##++++##++#.',
    '.#++++++++++++#.',
    '.#++++####++++#.',
    '.#++++++++++++#.',
    '.##++++++++++##.',
    '..#++++++++++#..',
    '...#--------#...',
    '....########....',
])


for name, (pal_name, rows) in ART.items():
    pal = PAL[pal_name]
    img = Image.new('RGB', (16, 16))
    px = img.load()
    for y, row in enumerate(rows):
        assert len(row) == 16, (name, y, len(row))
        for x, ch in enumerate(row):
            px[x, y] = pal[LEGEND[ch]]
    path = os.path.join(OUT, name + '.png')
    img.save(path)
    print('wrote %s  (palette %s, %d colours used)'
          % (os.path.relpath(path, ROOT), pal_name, len(set(img.getdata()))))
