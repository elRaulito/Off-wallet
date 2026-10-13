"""Compile content/story.txt into the ROM's screens, dialogue and characters.

Emits:
  src/story_data.c   screens, dialogue boxes, characters, palettes  (bank 1)
  src/story_data.h   the ids the game code refers to
  src/story_tiles.c  tiles for custom sprites drawn as ASCII art    (bank 5)

Everything is validated here so that a content mistake stops the build with a
readable message instead of producing a ROM with text chopped off at the screen
edge -- which is exactly what happened before this file existed.
"""
import io, os, re, sys
try:
    from PIL import Image
except ImportError:
    Image = None

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'content', 'story.txt')

SCREEN_W = 20
SCREEN_H = 18
DIALOG_X = 1
DIALOG_W = 18          # the box borders occupy columns 0 and 19
DIALOG_ROWS = 3
BUDGET = 9000          # bytes of bank 1 we are willing to spend

# All artwork now comes from the content file: either a PNG under content/art
# or ASCII art written inline. Nothing is built into the code.
BUILTIN_SPRITES = {}
DEFAULT_SPRITE_PAL = {}
MAX_SPRITE_TILES = 16      # up to 4x4 tiles per sprite
# Palettes all come from the content file now, including the ones the text
# uses; these three names are required because the draw commands use them.
BUILTIN_PALETTES = {}
REQUIRED_PALETTES = ['text', 'warn', 'accent']
MAX_PALETTES = 8       # the CGB has 8 background palettes
ART_CHARS = {'.': 0, '-': 1, '+': 2, '#': 3}

# Sections the game code looks up by name. Contents are free to change.
REQUIRED_SCREENS = [
    'title', 'starter', 'egg_swift', 'egg_ancient', 'flip_coin', 'hatch',
    'hatch_key', 'hatch_stretch', 'hatch_path', 'wallet', 'address',
    'secret_wif', 'secret_words', 'output_key', 'receive', 'receive_wait',
    'utxo_list', 'utxo_txid', 'utxo_vout', 'utxo_value',
]
REQUIRED_DIALOGS = [
    'intro', 'starter_ok', 'starter_locked', 'starter_chosen', 'flips_done',
    'congrats', 'hatch_failed', 'saved', 'welcome_back',
]
REQUIRED_CHARS = ['prof']
REQUIRED_CONFIG = [
    'starter_unlocked', 'starter_cursor_0', 'starter_cursor_1',
    'starter_cursor_2', 'wallet_menu_x', 'wallet_menu_y', 'wallet_menu_step',
    'wallet_menu_count', 'flip_grid', 'flip_grid_cols', 'flip_count_y',
    'flip_stats_y', 'flip_prompt_y',
]

errors = []


def err(line_no, msg):
    errors.append('content/story.txt:%d: %s' % (line_no, msg))


# ----------------------------------------------------------------- parsing
def parse(path):
    sections = []           # (kind, id, [(line_no, raw_line)])
    cur = None
    for n, raw in enumerate(io.open(path, encoding='utf-8'), 1):
        line = raw.rstrip('\n').rstrip('\r')
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue
        m = re.match(r'^\[(\w+)\s+([\w.]+)\]$', stripped)
        if m:
            cur = (m.group(1), m.group(2), [])
            sections.append(cur)
            continue
        m = re.match(r'^\[(\w+)\]$', stripped)
        if m:
            cur = (m.group(1), '', [])
            sections.append(cur)
            continue
        if cur is None:
            err(n, 'content outside any [section]')
            continue
        cur[2].append((n, line))
    return sections


def split_key(line):
    m = re.match(r'^\s*([\w.]+)\s*=\s*(.*)$', line)
    if m:
        return m.group(1), m.group(2).strip()
    return None, None


def parse_xy(val, line_no, what):
    m = re.match(r'^(\d+)\s*,\s*(\d+)$', val.strip())
    if not m:
        err(line_no, '%s: expected "x,y", got %r' % (what, val))
        return 0, 0
    x, y = int(m.group(1)), int(m.group(2))
    if x >= SCREEN_W or y >= SCREEN_H:
        err(line_no, '%s: %d,%d is off a %dx%d screen' % (what, x, y, SCREEN_W, SCREEN_H))
    return x, y


# ----------------------------------------------------------------- model
palettes = dict(BUILTIN_PALETTES)      # id -> index
palette_colors = {}                    # index -> (r,g,b) x4 in 5-bit, custom
palette_colors_by_id = {}              # id -> (r,g,b) x4 in 8-bit, for PNGs
sprites = dict(BUILTIN_SPRITES)        # id -> C expression for the tile base
sprite_tiles = []                      # custom sprite tile data, in order
sprite_size = {}                       # id -> (tiles wide, tiles high)
sprite_pal = {}                        # id -> (palette id, line no)
sprite_auto_pal = {}                   # id -> 4 colours taken from a PNG
characters = []                        # (id, name, sprite_id, pal_id, px, py)
screens = []                           # (id, clear, [ops])
dialogs = []                           # (id, [boxes]) ; box = (speaker, l1,l2,l3)
config = {}

sections = parse(SRC)


def hex_color(tok, line_no):
    m = re.match(r'^#([0-9a-fA-F]{6})$', tok)
    if not m:
        err(line_no, 'colour %r is not #RRGGBB' % tok)
        return (0, 0, 0)
    v = int(m.group(1), 16)
    # 8-bit per channel down to the CGB's 5 bits
    return ((v >> 19) & 31, (v >> 11) & 31, (v >> 3) & 31)


def lum(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def load_ascii(sid, rows):
    """16 rows of 16 characters of . - + # -> colour indices."""
    grid = []
    for n, r in rows:
        if len(r) != 16:
            err(n, 'sprite row is %d characters, ASCII art needs exactly 16 '
                   '(use "png = art/%s.png" for other sizes)' % (len(r), sid))
            r = (r + '.' * 16)[:16]
        bad = [c for c in r if c not in ART_CHARS]
        if bad:
            err(n, 'sprite row uses %r; only . - + # are allowed'
                % ''.join(sorted(set(bad))))
            r = ''.join(c if c in ART_CHARS else '.' for c in r)
        grid.append([ART_CHARS[c] for c in r])
    if len(grid) != 16:
        err(rows[0][0] if rows else 0,
            'sprite %r has %d rows of ASCII art, needs 16' % (sid, len(grid)))
        while len(grid) < 16:
            grid.append([0] * 16)
    return grid[:16]


def load_png(sid, png_path, pal_ref):
    """A PNG of at most four colours -> colour indices, plus the palette those
    colours form (lightest first, which is what index 0 means on screen)."""
    rel, n = png_path
    path = os.path.join(ROOT, 'content', rel.replace('/', os.sep))
    if Image is None:
        err(n, 'reading PNGs needs Pillow: python -m pip install pillow')
        return [[0] * 16 for _ in range(16)], None
    if not os.path.exists(path):
        err(n, 'no such image: content/%s' % rel)
        return [[0] * 16 for _ in range(16)], None
    img = Image.open(path).convert('RGBA')
    w, h = img.size
    if w % 8 or h % 8 or w == 0 or h == 0:
        err(n, '%s is %dx%d; width and height must be multiples of 8' % (rel, w, h))
        img = img.resize(((w // 8 or 1) * 8, (h // 8 or 1) * 8))
        w, h = img.size
    px = img.load()

    # transparent pixels count as the background colour
    snap = {}
    seen = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            c = None if a < 128 else (r, g, b)
            if c is not None and c not in seen:
                seen.append(c)
    if len(seen) > 4:
        # A Game Boy tile has four colours. Rather than refuse the image --
        # painting tools love to leave a stray anti-aliased pixel behind --
        # keep the four most used colours and snap the rest to the nearest of
        # them, and say so.
        counts = {}
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                if a >= 128:
                    counts[(r, g, b)] = counts.get((r, g, b), 0) + 1
        keep = sorted(counts, key=lambda c: -counts[c])[:4]
        dropped = [c for c in seen if c not in keep]
        print('note: %s uses %d colours; keeping the four most common and '
              'snapping %d stray pixel colour(s) to them'
              % (rel, len(seen), len(dropped)))
        for c in dropped:
            best, bc = None, keep[0]
            for t in keep:
                d = sum((u - v) ** 2 for u, v in zip(c, t))
                if best is None or d < best:
                    best, bc = d, t
            print('      #%02x%02x%02x (%d px) -> #%02x%02x%02x'
                  % (c[0], c[1], c[2], counts[c], bc[0], bc[1], bc[2]))
            snap[c] = bc
        seen = keep

    if pal_ref is not None and pal_ref[0] != 'auto':
        # map each colour onto the nearest entry of the named palette
        pid = pal_ref[0]
        if pid not in palette_colors_by_id:
            err(pal_ref[1], 'sprite %r uses palette %r, which has no colours '
                            'defined here (built-in palettes cannot be matched '
                            'against a PNG; use "palette = auto")' % (sid, pid))
            order = sorted(seen, key=lum, reverse=True)
            index_of = {c: min(i, 3) for i, c in enumerate(order)}
            auto = None
        else:
            target = palette_colors_by_id[pid]      # 4 x (r,g,b) in 8-bit
            index_of = {}
            for c in seen:
                best, bi = None, 0
                for i, t in enumerate(target):
                    d = sum((a - b) ** 2 for a, b in zip(c, t))
                    if best is None or d < best:
                        best, bi = d, i
                index_of[c] = bi
            auto = None
    else:
        # the PNG defines its own palette: lightest colour becomes index 0
        order = sorted(seen, key=lum, reverse=True)
        while len(order) < 4:
            order.append(order[-1] if order else (0, 0, 0))
        index_of = {c: order.index(c) for c in seen}
        auto = order[:4]

    grid = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            c = snap.get((r, g, b), (r, g, b))
            grid[y][x] = 0 if a < 128 else index_of.get(c, 0)
    return grid, auto


for kind, sid, body in sections:
    if kind == 'config':
        for n, line in body:
            k, v = split_key(line)
            if k is None:
                err(n, 'expected "key = value" in [config]')
            else:
                config[k] = (v, n)

    elif kind == 'palette':
        cols = None
        for n, line in body:
            k, v = split_key(line)
            if k == 'colors':
                toks = v.split()
                if len(toks) != 4:
                    err(n, 'colors needs exactly 4 values, got %d' % len(toks))
                cols = [hex_color(t, n) for t in toks[:4]]
            elif k is not None:
                err(n, 'unknown key %r in [palette]' % k)
        if cols is None:
            err(body[0][0] if body else 0, 'palette %r has no "colors" line' % sid)
            cols = [(0, 0, 0)] * 4
        idx = len(BUILTIN_PALETTES) + len(palette_colors)
        if idx >= MAX_PALETTES:
            err(body[0][0] if body else 0,
                'too many palettes: the hardware has %d' % MAX_PALETTES)
            idx = 0
        palettes[sid] = idx
        palette_colors[idx] = cols
        palette_colors_by_id[sid] = [(r * 255 // 31, g * 255 // 31, b * 255 // 31)
                                     for (r, g, b) in cols]

    elif kind == 'sprite':
        png_path, pal_ref, art_rows = None, None, []
        for n, line in body:
            k, v = split_key(line)
            if k == 'png':
                png_path = (v, n)
            elif k == 'palette':
                pal_ref = (v, n)
            elif k is not None:
                err(n, 'unknown key %r in [sprite]' % k)
            else:
                art_rows.append((n, line.strip()))

        first_line = body[0][0] if body else 0
        grid = None       # grid[y][x] = colour index 0..3
        if png_path and art_rows:
            err(first_line, 'sprite %r has both a png and ASCII art; pick one' % sid)
        if png_path:
            grid, pal_from_png = load_png(sid, png_path, pal_ref)
        elif art_rows:
            grid = load_ascii(sid, art_rows)
            pal_from_png = None
        else:
            err(first_line, 'sprite %r is empty: needs "png = ..." or ASCII art' % sid)
            grid, pal_from_png = [[0] * 16 for _ in range(16)], None

        h_px, w_px = len(grid), len(grid[0])
        tw, th = w_px // 8, h_px // 8
        if tw * th > MAX_SPRITE_TILES:
            err(first_line, 'sprite %r is %dx%d tiles, the limit is %d tiles'
                % (sid, tw, th, MAX_SPRITE_TILES))
        # tiles in reading order: row of tiles at a time, left to right
        tiles = []
        for ty in range(th):
            for tx in range(tw):
                t = bytearray()
                for y in range(ty * 8, ty * 8 + 8):
                    lo = hi = 0
                    for i, x in enumerate(range(tx * 8, tx * 8 + 8)):
                        v = grid[y][x]
                        if v & 1:
                            lo |= 0x80 >> i
                        if v & 2:
                            hi |= 0x80 >> i
                    t.append(lo)
                    t.append(hi)
                tiles.append(bytes(t))
        base = len(sprite_tiles)
        sprite_tiles.extend(tiles)
        sprites[sid] = 'STORY_TILE_BASE + %d' % base
        sprite_size[sid] = (tw, th)
        if pal_from_png is not None:
            sprite_auto_pal[sid] = pal_from_png
        if pal_ref is not None:
            sprite_pal[sid] = pal_ref

    elif kind == 'character':
        name, spr, pal, px, py = None, None, 'text', 0, 0
        for n, line in body:
            k, v = split_key(line)
            if k == 'name':
                name = v
            elif k == 'sprite':
                spr = (v, n)
            elif k == 'palette':
                pal = (v, n)
            elif k == 'portrait':
                px, py = parse_xy(v, n, 'portrait')
            elif k is not None:
                err(n, 'unknown key %r in [character]' % k)
        characters.append((sid, name, spr, pal, px, py, body[0][0] if body else 0))

    elif kind == 'screen':
        clear = True
        ops = []
        for n, line in body:
            k, v = split_key(line)
            if k == 'clear':
                clear = v.lower() not in ('no', 'false', '0')
                continue
            if k is not None and k in ('text', 'accent', 'warn', 'sprite', 'blank'):
                err(n, 'write draw commands as "%s x,y ..." without "="' % k)
                continue
            parts = line.strip().split(None, 2)
            if not parts:
                continue
            cmd = parts[0]
            if cmd in ('text', 'accent', 'warn'):
                if len(parts) < 3:
                    err(n, '%s needs "x,y <string>"' % cmd)
                    continue
                x, y = parse_xy(parts[1], n, cmd)
                # keep the string exactly as written, after the coordinates
                s = line.rstrip()
                s = s[s.index(parts[1]) + len(parts[1]):]
                s = s[1:] if s.startswith(' ') else s
                if len(s) > SCREEN_W - x:
                    err(n, 'text at column %d is %d characters: %d would run off '
                           'the screen (max %d here)'
                        % (x, len(s), x + len(s), SCREEN_W - x))
                    s = s[:SCREEN_W - x]
                # the palette is resolved by name after parsing
                ops.append(('TEXT', x, y, cmd, s))
            elif cmd == 'sprite':
                toks = line.strip().split()
                if len(toks) < 3:
                    err(n, 'sprite needs "x,y <sprite id> [palette id]"')
                    continue
                x, y = parse_xy(toks[1], n, 'sprite')
                ref = toks[2]
                # sprite and palette names are resolved after parsing, so
                # sections may appear in any order
                pal_id = toks[3] if len(toks) > 3 else None
                ops.append(('SPRITE', x, y, ref, pal_id, n))
            elif cmd == 'blank':
                m = re.match(r'^(\d+)\s*,\s*(\d+)\s*,\s*(\d+)$', parts[1]
                             if len(parts) > 1 else '')
                if not m:
                    err(n, 'blank needs "x,y,width"')
                    continue
                x, y, w = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if x + w > SCREEN_W:
                    err(n, 'blank of %d tiles at column %d runs off the screen' % (w, x))
                    w = SCREEN_W - x
                ops.append(('BLANK', x, y, w, None))
            else:
                err(n, 'unknown draw command %r' % cmd)
        screens.append((sid, clear, ops))

    elif kind == 'dialog':
        boxes = []
        speaker = None
        lines = []
        first_line_no = body[0][0] if body else 0

        def flush(n):
            if lines:
                boxes.append((speaker, list(lines), n))
                del lines[:]

        for n, line in body:
            s = line.strip()
            if s == '--':
                flush(n)
                continue
            k, v = split_key(line)
            if k == 'speaker':
                if lines:
                    flush(n)
                speaker = v
                continue
            if k is not None:
                err(n, 'unknown key %r in [dialog]' % k)
                continue
            if len(lines) >= DIALOG_ROWS:
                err(n, 'a dialogue box holds %d lines; start a new one with "--"'
                    % DIALOG_ROWS)
                continue
            if len(s) > DIALOG_W:
                err(n, 'dialogue line is %d characters, the box fits %d: %r'
                    % (len(s), DIALOG_W, s))
                s = s[:DIALOG_W]
            lines.append(s)
        flush(first_line_no)
        if not boxes:
            err(first_line_no, 'dialog %r has no text' % sid)
        dialogs.append((sid, boxes))

    else:
        err(body[0][0] if body else 0, 'unknown section [%s]' % kind)

# ------------------------------------------------- resolve deferred names
# A PNG can bring its own four colours ("palette = auto", the default): give
# each such sprite a hardware palette slot now that all [palette] sections
# have been seen.
sprite_pal_index = {}
for sid_ in sorted(sprite_auto_pal):
    cols8 = sprite_auto_pal[sid_]
    cols5 = [(r * 31 // 255, g * 31 // 255, b * 31 // 255) for (r, g, b) in cols8]
    # reuse an identical palette rather than burning a second slot on it
    match = None
    for idx, c in palette_colors.items():
        if list(c) == cols5:
            match = idx
            break
    if match is not None:
        sprite_pal_index[sid_] = match
        continue
    idx = len(BUILTIN_PALETTES) + len(palette_colors)
    if idx >= MAX_PALETTES:
        errors.append(
            'content/story.txt: sprite %r wants its own palette, but all %d '
            'background palettes the hardware has are already taken (%d are '
            'spent on text). Add a line "palette = <name>" to [sprite %s] to '
            'share an existing one: "warn" is red, "accent" is orange, or '
            'name another sprite palette.' % (sid_, MAX_PALETTES,
                                              len(BUILTIN_PALETTES), sid_))
        idx = 0
    else:
        palette_colors[idx] = cols5
    sprite_pal_index[sid_] = idx

for sid_, (pid, pn) in sprite_pal.items():
    if pid == 'auto':
        continue
    if pid not in palettes:
        err(pn, 'sprite %r uses unknown palette %r (have: %s)'
            % (sid_, pid, ', '.join(sorted(palettes))))
    else:
        sprite_pal_index[sid_] = palettes[pid]

for r in REQUIRED_PALETTES:
    if r not in palettes:
        errors.append('content/story.txt: [palette %s] is required: the draw '
                      'commands use it' % r)

resolved = []
for (sid_, clear, ops) in screens:
    out = []
    for o in ops:
        if o[0] == 'TEXT':
            # o[3] is the command name; map it onto a palette index
            out.append(('TEXT', o[1], o[2], palettes.get(o[3], 0), o[4]))
            continue
        if o[0] != 'SPRITE':
            out.append(o)
            continue
        _, x, y, ref, pal_id, n = o
        if ref not in sprites:
            err(n, 'unknown sprite %r (have: %s)'
                % (ref, ', '.join(sorted(sprites)) or 'none defined'))
            out.append(('TEXT', x, y, 0, '?'))
            continue
        tw, th = sprite_size.get(ref, (2, 2))
        if x + tw > SCREEN_W or y + th > SCREEN_H:
            err(n, 'sprite %r is %dx%d tiles and does not fit at %d,%d'
                % (ref, tw, th, x, y))
        if pal_id is None:
            pidx = sprite_pal_index.get(ref, 0)
        elif pal_id not in palettes:
            err(n, 'unknown palette %r (have: %s)'
                % (pal_id, ', '.join(sorted(palettes))))
            pidx = 0
        else:
            pidx = palettes[pal_id]
        out.append(('SPRITE', x, y, ref, pidx))
    resolved.append((sid_, clear, out))
screens = resolved

# ----------------------------------------------------------- cross-checks
char_ids = [c[0] for c in characters]
resolved_chars = []
for (cid, name, spr, pal, px, py, n) in characters:
    tile = 'T_BTC_TL'
    if spr is None:
        err(n, 'character %r has no "sprite" line' % cid)
    else:
        ref, sn = spr
        if ref not in sprites:
            err(sn, 'character %r uses unknown sprite %r' % (cid, ref))
        else:
            tile = sprites[ref]
    pidx = sprite_pal_index.get(spr[0] if spr else '', 0)
    if isinstance(pal, tuple):
        ref, pn = pal
        if ref == 'auto':
            pass                      # keep the sprite's own colours
        elif ref not in palettes:
            err(pn, 'character %r uses unknown palette %r' % (cid, ref))
        else:
            pidx = palettes[ref]
    if name is not None and len(name) + 2 > SCREEN_W - 1:
        err(n, 'character name %r is too long to fit the name plate' % name)
    tw, th = sprite_size.get(spr[0] if spr else '', (2, 2))
    resolved_chars.append((cid, name, tile, pidx, px, py, tw, th))

screen_ids = [s[0] for s in screens]
dialog_ids = [d[0] for d in dialogs]
for want, have, what in ((REQUIRED_SCREENS, screen_ids, 'screen'),
                         (REQUIRED_DIALOGS, dialog_ids, 'dialog'),
                         (REQUIRED_CHARS, char_ids, 'character')):
    for r in want:
        if r not in have:
            errors.append('content/story.txt: the game code needs [%s %s]; '
                          'it is missing (do not rename REQUIRED sections)' % (what, r))
for group, what in ((screen_ids, 'screen'), (dialog_ids, 'dialog'),
                    (char_ids, 'character')):
    seen = set()
    for i in group:
        if i in seen:
            errors.append('content/story.txt: [%s %s] is defined twice' % (what, i))
        seen.add(i)

for r in REQUIRED_CONFIG:
    if r not in config:
        errors.append('content/story.txt: [config] is missing %r' % r)

for (sid, boxes) in dialogs:
    for (sp, lines, n) in boxes:
        if sp is not None and sp not in char_ids:
            err(n, 'dialog %r has speaker %r, which is not a [character]' % (sid, sp))

if errors:
    print('content errors:\n')
    for e in errors:
        print('  ' + e)
    print('\n%d problem(s); nothing was written.' % len(errors))
    sys.exit(1)

# ----------------------------------------------------------------- emit
def cstr(s):
    out = s.replace('\\', '\\\\').replace('"', '\\"')
    return '"%s"' % out


def ident(s):
    return re.sub(r'\W', '_', s).upper()


ops_flat = []
screen_rows = []
for (sid, clear, ops) in screens:
    first = len(ops_flat)
    for o in ops:
        ops_flat.append(o)
    screen_rows.append((sid, first, len(ops), clear))

box_flat = []
dialog_rows = []
for (sid, boxes) in dialogs:
    first = len(box_flat)
    for (sp, lines, n) in boxes:
        box_flat.append((sp, lines))
    dialog_rows.append((sid, first, len(boxes)))

c = []
c.append('/* AUTO-GENERATED from content/story.txt by tools/gen/gen_story.py.')
c.append('   Edit the content file, not this one. */')
c.append('#pragma bank 1')
c.append('#include <gb/gb.h>')
c.append('#include <gb/cgb.h>')          # RGB()
c.append('#include "story.h"')
c.append('#include "ui.h"')              # GFX_BASE
c.append('#include "gfx/gfx_tiles.h"')   # T_*_TL
c.append('')
c.append('const story_char STORY_CHARS[%d] = {' % max(1, len(resolved_chars)))
for (cid, name, tile, pidx, px, py, tw, th) in resolved_chars:
    c.append('  { %-22s %-24s %d, %2d, %2d, 0x%X%X },   /* %s */'
             % ((cstr(name) + ',') if name else '0,', str(tile) + ',',
                pidx, px, py, tw, th, cid))
if not resolved_chars:
    c.append('  { 0, 0, 0, 0, 0, 0x22 },')
c.append('};')
c.append('')
c.append('const story_op STORY_OPS[%d] = {' % max(1, len(ops_flat)))
for o in ops_flat:
    if o[0] == 'TEXT':
        c.append('  { SOP_TEXT,   %2d, %2d, %d, %s },' % (o[1], o[2], o[3], cstr(o[4])))
    elif o[0] == 'SPRITE':
        c.append('  { SOP_SPRITE, %2d, %2d, %d, 0 },   /* %s */'
                 % (o[1], o[2], o[4], o[3]))
    else:
        c.append('  { SOP_BLANK,  %2d, %2d, %d, 0 },' % (o[1], o[2], o[3]))
if not ops_flat:
    c.append('  { SOP_END, 0, 0, 0, 0 },')
c.append('};')
c.append('')
c.append('/* Sprite ops need a tile base that is only known at compile time, so')
c.append('   they are kept in a parallel table rather than inside story_op. */')
c.append('/* size of each sprite op, tiles wide in the high nibble. */')
c.append('const uint8_t STORY_OP_SIZE[%d] = {' % max(1, len(ops_flat)))
srow = []
for o in ops_flat:
    if o[0] == 'SPRITE':
        tw, th = sprite_size.get(o[3], (2, 2))
        srow.append('0x%X%X' % (tw, th))
    else:
        srow.append('0')
c.append('  ' + ', '.join(srow) if srow else '  0')
c.append('};')
c.append('')
c.append('const uint8_t STORY_OP_TILE[%d] = {' % max(1, len(ops_flat)))
row = []
for o in ops_flat:
    # o[3] holds the sprite id; resolve it to its tile-base expression
    row.append(('(uint8_t)(%s)' % sprites[o[3]]) if o[0] == 'SPRITE' else '0')
c.append('  ' + ', '.join(row) if row else '  0')
c.append('};')
c.append('')
c.append('const story_screen STORY_SCREENS[%d] = {' % max(1, len(screen_rows)))
for (sid, first, count, clear) in screen_rows:
    c.append('  { %3d, %2d, %d },   /* %s */' % (first, count, 1 if clear else 0, sid))
c.append('};')
c.append('')
c.append('const story_box STORY_BOXES[%d] = {' % max(1, len(box_flat)))
for (sp, lines) in box_flat:
    sp_idx = char_ids.index(sp) if sp in char_ids else 255
    l = [cstr(x) for x in lines] + ['0'] * (3 - len(lines))
    c.append('  { %3d, %s, %s, %s },' % (sp_idx, l[0], l[1], l[2]))
c.append('};')
c.append('')
c.append('const story_dialog STORY_DIALOGS[%d] = {' % max(1, len(dialog_rows)))
for (sid, first, count) in dialog_rows:
    c.append('  { %3d, %d },   /* %s */' % (first, count, sid))
c.append('};')
c.append('')
npal = len(palette_colors)
c.append('/* Extra CGB background palettes defined in the content file. */')
c.append('const uint16_t STORY_PALETTES[%d] = {' % max(1, npal * 4))
for idx in sorted(palette_colors):
    cols = palette_colors[idx]
    c.append('  ' + ', '.join('RGB(%d,%d,%d)' % rgb for rgb in cols) + ',')
if not npal:
    c.append('  0,')
c.append('};')
c.append('const uint8_t STORY_PALETTE_FIRST = %d;' % len(BUILTIN_PALETTES))
c.append('const uint8_t STORY_PALETTE_COUNT = %d;' % npal)
io.open(os.path.join(ROOT, 'src', 'story_data.c'), 'w', newline='\n').write(
    '\n'.join(c) + '\n')

h = []
h.append('/* AUTO-GENERATED ids from content/story.txt. */')
h.append('#ifndef OW_STORY_DATA_H')
h.append('#define OW_STORY_DATA_H')
h.append('')
for i, (sid, _, _, _) in enumerate(screen_rows):
    h.append('#define SCR_%-16s %d' % (ident(sid), i))
h.append('')
for i, (sid, _, _) in enumerate(dialog_rows):
    h.append('#define DLG_%-16s %d' % (ident(sid), i))
h.append('')
for i, cch in enumerate(resolved_chars):
    h.append('#define CHR_%-16s %d' % (ident(cch[0]), i))
h.append('')
h.append('#define STORY_NSCREENS   %d' % len(screen_rows))
h.append('#define STORY_NDIALOGS   %d' % len(dialog_rows))
h.append('#define STORY_NCHARS     %d' % len(resolved_chars))
h.append('#define STORY_NSPRITES   %d' % (len(sprite_tiles) // 4))
h.append('#define STORY_NTILES     %d' % len(sprite_tiles))
h.append('#define STORY_NPALETTES  %d' % npal)
h.append('')
h.append('/* palette slots, as allocated from the content file */')
for _nm in ('text', 'warn', 'accent'):
    h.append('#define PAL_%-10s %d' % (_nm.upper(), palettes.get(_nm, 0)))
h.append('')
h.append('/* [config] values */')


def cfg(name, default='0'):
    if name in config:
        return config[name][0]
    return default


h.append('#define CFG_STARTER_UNLOCKED %s' % cfg('starter_unlocked'))
for i in range(3):
    v = cfg('starter_cursor_%d' % i, '0,0').replace(' ', '')
    x, y = v.split(',')
    h.append('#define CFG_STARTER_CUR%d_X   %s' % (i, x))
    h.append('#define CFG_STARTER_CUR%d_Y   %s' % (i, y))
h.append('#define CFG_MENU_X           %s' % cfg('wallet_menu_x'))
h.append('#define CFG_MENU_Y           %s' % cfg('wallet_menu_y'))
h.append('#define CFG_MENU_STEP        %s' % cfg('wallet_menu_step'))
h.append('#define CFG_MENU_COUNT       %s' % cfg('wallet_menu_count', '4'))
_fx, _fy = cfg('flip_grid', '2,4').replace(' ', '').split(',')
h.append('#define CFG_FLIP_X           %s' % _fx)
h.append('#define CFG_FLIP_Y           %s' % _fy)
h.append('#define CFG_FLIP_COLS        %s' % cfg('flip_grid_cols', '16'))
h.append('#define CFG_FLIP_COUNT_Y     %s' % cfg('flip_count_y', '13'))
h.append('#define CFG_FLIP_STATS_Y     %s' % cfg('flip_stats_y', '14'))
h.append('#define CFG_FLIP_PROMPT_Y    %s' % cfg('flip_prompt_y', '16'))
h.append('')
h.append('#endif')
io.open(os.path.join(ROOT, 'src', 'story_data.h'), 'w', newline='\n').write(
    '\n'.join(h) + '\n')

t = []
t.append('/* AUTO-GENERATED sprite tiles from content/story.txt. */')
t.append('#ifdef OW_GAMEBOY')
t.append('#pragma bank 5')
t.append('#endif')
t.append('#include <stdint.h>')
t.append('')
blob = b''.join(sprite_tiles)
t.append('const uint8_t STORY_TILES[%d] = {' % max(1, len(blob)))
if blob:
    for i in range(0, len(blob), 16):
        t.append('  ' + ','.join('0x%02x' % b for b in blob[i:i + 16]) + ',')
else:
    t.append('  0,')
t.append('};')
io.open(os.path.join(ROOT, 'src', 'story_tiles.c'), 'w', newline='\n').write(
    '\n'.join(t) + '\n')

# ----------------------------------------------------------------- report
text_bytes = sum(len(o[4]) + 1 for o in ops_flat if o[0] == 'TEXT')
text_bytes += sum(sum(len(x) + 1 for x in lines) for (_, lines) in box_flat)
table_bytes = len(ops_flat) * 7 + len(screen_rows) * 3 + len(box_flat) * 7 \
    + len(dialog_rows) * 2 + len(resolved_chars) * 6
total = text_bytes + table_bytes
print('story: %d screens (%d draw ops), %d dialogue boxes in %d conversations, '
      '%d characters' % (len(screen_rows), len(ops_flat), len(box_flat),
                         len(dialog_rows), len(resolved_chars)))
print('       %d custom sprites (%d tiles), %d extra palettes'
      % (len(sprite_tiles) // 4, len(sprite_tiles), npal))
print('       about %d bytes of bank 1 (%d text + %d tables), budget %d'
      % (total, text_bytes, table_bytes, BUDGET))
if total > BUDGET:
    print('WARNING: that is more than the %d bytes bank 1 has spare; the link '
          'may overflow.' % BUDGET)
print('       wrote src/story_data.c, src/story_data.h, src/story_tiles.c')
