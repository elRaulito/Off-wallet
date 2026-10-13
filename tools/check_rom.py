"""Validate a Game Boy ROM header: title, CGB flag, cartridge type, sizes and
both checksums. A flash cart will refuse or misconfigure a ROM whose header
does not match what the file actually is."""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else 'build/offwallet.gbc'
rom = open(path, 'rb').read()

CART = {0x00: 'ROM only', 0x19: 'MBC5', 0x1A: 'MBC5+RAM',
        0x1B: 'MBC5+RAM+BATTERY', 0x1C: 'MBC5+RUMBLE'}
ROM_SIZE = {0: '32K', 1: '64K', 2: '128K', 3: '256K', 4: '512K', 5: '1M', 6: '2M'}
RAM_SIZE = {0: 'none', 1: '2K', 2: '8K', 3: '32K', 4: '128K', 5: '64K'}

title = rom[0x134:0x13F].rstrip(b'\0').decode('ascii', 'replace')
cgb = rom[0x143]
cart = rom[0x147]
romsz = rom[0x148]
ramsz = rom[0x149]

hdr = 0
for b in rom[0x134:0x14D]:
    hdr = (hdr - b - 1) & 0xFF
glob = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF

print('file            %s (%d bytes = %d banks)' % (path, len(rom), len(rom) // 16384))
print('title           %r' % title)
print('CGB flag        0x%02X (%s)' % (cgb, {0x80: 'CGB enhanced, DMG compatible',
                                             0xC0: 'CGB only'}.get(cgb, 'DMG only')))
print('cartridge type  0x%02X (%s)' % (cart, CART.get(cart, 'unknown')))
print('ROM size byte   0x%02X (%s declared)' % (romsz, ROM_SIZE.get(romsz, '?')))
print('RAM size byte   0x%02X (%s)' % (ramsz, RAM_SIZE.get(ramsz, '?')))

ok = True
def chk(label, got, exp):
    global ok
    good = got == exp
    if not good:
        ok = False
    print('%-15s %s (got 0x%X, expected 0x%X)'
          % (label, 'ok' if good else 'BAD', got, exp))

chk('header cksum', rom[0x14D], hdr)
chk('global cksum', (rom[0x14E] << 8) | rom[0x14F], glob)

declared = 32768 << romsz
if declared != len(rom):
    print('size mismatch   declared %d bytes, file is %d' % (declared, len(rom)))
    ok = False
else:
    print('size mismatch   none')

if ramsz == 0 and cart == 0x1B:
    print('warning: cartridge claims battery-backed RAM but declares no RAM')
    ok = False

print()
print('HEADER OK' if ok else 'HEADER PROBLEMS')
sys.exit(0 if ok else 1)
