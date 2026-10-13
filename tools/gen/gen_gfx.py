"""Hand-drawn tiles for OffWallet, written as ASCII art and converted to
Game Boy 2bpp. Legend: '.' = colour 0, '-' = 1, '+' = 2, '#' = 3."""
import io

LEGEND = {'.': 0, '-': 1, '+': 2, '#': 3}

def tile(rows):
    assert len(rows) == 8, rows
    out = bytearray()
    for r in rows:
        assert len(r) == 8, r
        lo = hi = 0
        for x, ch in enumerate(r):
            v = LEGEND[ch]
            if v & 1: lo |= 0x80 >> x
            if v & 2: hi |= 0x80 >> x
        out.append(lo)
        out.append(hi)
    return bytes(out)

TILES = {}

# ---- dialogue box frame (Pokemon-style double border) ----
TILES['BOX_TL'] = ['########','#------#','#-####-#','#-#....#','#-#.....','#-#.....','#-#.....','#-#.....']
TILES['BOX_T']  = ['########','--------','####++##','........','........','........','........','........']
TILES['BOX_TR'] = ['########','#------#','#-####-#','#....#-#','.....#-#','.....#-#','.....#-#','.....#-#']
TILES['BOX_L']  = ['#-#.....','#-#.....','#-#.....','#-#.....','#-#.....','#-#.....','#-#.....','#-#.....']
TILES['BOX_R']  = ['.....#-#','.....#-#','.....#-#','.....#-#','.....#-#','.....#-#','.....#-#','.....#-#']
TILES['BOX_BL'] = ['#-#.....','#-#.....','#-####-#','#------#','########','........','........','........']
TILES['BOX_B']  = ['........','........','########','--------','########','........','........','........']
TILES['BOX_BR'] = ['.....#-#','.....#-#','#-####-#','#------#','########','........','........','........']
TILES['BLANK']  = ['........'] * 8

# ---- Poke Ball, 2x2 tiles (16x16), palette 1: 0=white 1=red 2=black 3=dark ----
TILES['BALL_TL'] = ['....++++','..++####','.+######','.+######','+#######','+#######','+#######','+#######']
TILES['BALL_TR'] = ['++++....','####++..','######+.','######+.','#######+','#######+','#######+','#######+']
TILES['BALL_BL'] = ['++++++++','..++++++','.+......','.+..--++','.+..--++','.+......','..++++++','....++++']
TILES['BALL_BR'] = ['++++++++','++++++..','......+.','++--..+.','++--..+.','......+.','++++++..','++++....']

# ---- egg, 2x2 tiles ----
TILES['EGG_TL'] = ['.....+++','...++###','..+#####','.+######','.+######','+#######','+#######','+#######']
TILES['EGG_TR'] = ['+++.....','###++...','#####+..','######+.','######+.','#######+','#######+','#######+']
TILES['EGG_BL'] = ['+#######','+#######','+#######','.+######','.+######','..+#####','...++###','.....+++']
TILES['EGG_BR'] = ['#######+','#######+','#######+','######+.','######+.','#####+..','###++...','+++.....']

# ---- bitcoin B, 2x2 tiles ----
TILES['BTC_TL'] = ['....+...','...++...','..+###..','.+#####.','+###..##','+##.....','+##.....','+##..###']
TILES['BTC_TR'] = ['..+.....','..++....','.###+...','#####+..','#...##+.','.....##+','.....##+','..####+.']
TILES['BTC_BL'] = ['+##..###','+##.....','+##.....','+###..##','.+#####.','..+###..','...++...','....+...']
TILES['BTC_BR'] = ['...###+.','.....##+','.....##+','#...##+.','#####+..','.###+...','..++....','..+.....']

# ---- progress bar pieces ----
TILES['BAR_EMPTY'] = ['########','#......#','#......#','#......#','#......#','#......#','#......#','########']
TILES['BAR_FULL']  = ['########','#.####.#','#.####.#','#.####.#','#.####.#','#.####.#','#.####.#','########']
TILES['CURSOR']    = ['........','..#.....','..##....','..###...','..####..','..###...','..##....','..#.....']

order = list(TILES.keys())
blob = bytearray()
for name in order:
    blob += tile(TILES[name])

c = ['/* AUTO-GENERATED tiles from tools/gen/gen_gfx.py. */',
     '#ifdef OW_GAMEBOY',
     '#pragma bank 5',
     '#endif',
     '#include <stdint.h>',
     '',
     'const uint8_t GFX_TILES[%d] = {' % len(blob)]
for i in range(0, len(blob), 16):
    c.append('  ' + ','.join('0x%02x' % b for b in blob[i:i+16]) + ',')
c.append('};')
io.open('../../src/gfx/gfx_tiles.c', 'w', newline='\n').write('\n'.join(c) + '\n')

h = ['/* AUTO-GENERATED tile indices. Tiles are loaded after the 95 font glyphs,',
     '   so tile id = GFX_BASE + one of these. */',
     '#ifndef OW_GFX_TILES_H',
     '#define OW_GFX_TILES_H',
     '#include <stdint.h>',
     '',
     '#define GFX_NTILES %d' % len(order),
     '#define GFX_BANK   5',
     '']
for i, name in enumerate(order):
    h.append('#define T_%-10s %d' % (name, i))
h += ['', 'extern const uint8_t GFX_TILES[%d];' % len(blob), '', '#endif']
io.open('../../src/gfx/gfx_tiles.h', 'w', newline='\n').write('\n'.join(h) + '\n')
print('wrote %d tiles (%d bytes): %s' % (len(order), len(blob), ', '.join(order)))
