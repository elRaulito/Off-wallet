"""Render an 8x8 1-bit font into Game Boy 2bpp tiles.

Glyphs come from a system monospace TTF rendered without antialiasing, then
hand-checked below by dumping them as ASCII art. ASCII 0x20..0x7E lands at
tile index (c - 0x20), so text can be drawn with set_bkg_tiles directly.
"""
import io, sys
from PIL import Image, ImageDraw, ImageFont

FIRST, LAST = 0x20, 0x7E
NGLYPH = LAST - FIRST + 1

def render(fontpath, size, dx, dy):
    font = ImageFont.truetype(fontpath, size)
    glyphs = []
    for c in range(FIRST, LAST + 1):
        img = Image.new('1', (8, 8), 0)
        d = ImageDraw.Draw(img)
        d.fontmode = '1'                      # no antialiasing
        d.text((dx, dy), chr(c), font=font, fill=1)
        glyphs.append(img)
    return glyphs

def to_2bpp(img):
    """Game Boy tile: 8 rows, each two bytes (low bitplane, high bitplane).
    A 1-bit glyph uses colour 3 (both planes set) on colour 0."""
    out = bytearray()
    px = img.load()
    for y in range(8):
        bits = 0
        for x in range(8):
            if px[x, y]:
                bits |= 0x80 >> x
        out.append(bits)      # plane 0
        out.append(bits)      # plane 1 -> colour index 3
    return bytes(out)

def ascii_art(img):
    px = img.load()
    return ['.'.join('') + ''.join('#' if px[x, y] else '.' for x in range(8))
            for y in range(8)]

FONT = 'C:/Windows/Fonts/lucon.ttf'
glyphs = render(FONT, 9, 0, -1)

# show a few glyphs so the result can be eyeballed rather than assumed
for ch in 'ABGQabgq0158:?':
    g = glyphs[ord(ch) - FIRST]
    print(ch, ' ', ' '.join(''.join('#' if g.load()[x, y] else '.' for x in range(8))
                            for y in range(0, 1)))
print()
for ch in 'Ag5':
    print('glyph %r' % ch)
    g = glyphs[ord(ch) - FIRST]
    for row in ascii_art(g):
        print('  ' + row)

blob = bytearray()
for g in glyphs:
    blob += to_2bpp(g)
assert len(blob) == NGLYPH * 16

out = ['/* AUTO-GENERATED 8x8 font, ASCII 0x%02X..0x%02X at tile index (c - 0x%02X).'
       % (FIRST, LAST, FIRST),
       '   Source: %s rendered at 9px with antialiasing off. */' % FONT.split('/')[-1],
       '#ifdef OW_GAMEBOY',
       '#pragma bank 5',
       '#endif',
       '#include <stdint.h>',
       '',
       '#define FONT_FIRST_CHAR 0x%02X' % FIRST,
       '#define FONT_NGLYPH     %d' % NGLYPH,
       '',
       'const uint8_t FONT_TILES[%d] = {' % len(blob)]
for i in range(0, len(blob), 16):
    out.append('  ' + ','.join('0x%02x' % b for b in blob[i:i+16]) + ',')
out.append('};')
io.open('../../src/gfx/font_tiles.c', 'w', newline='\n').write('\n'.join(out) + '\n')

hdr = ['/* AUTO-GENERATED font table. */',
       '#ifndef OW_FONT_TILES_H',
       '#define OW_FONT_TILES_H',
       '#include <stdint.h>',
       '',
       '#define FONT_FIRST_CHAR 0x%02X' % FIRST,
       '#define FONT_NGLYPH     %d' % NGLYPH,
       '#define FONT_BANK       5',
       '',
       'extern const uint8_t FONT_TILES[%d];' % len(blob),
       '',
       '#endif']
io.open('../../src/gfx/font_tiles.h', 'w', newline='\n').write('\n'.join(hdr) + '\n')
print()
print('wrote src/gfx/font_tiles.c: %d glyphs, %d bytes' % (NGLYPH, len(blob)))
