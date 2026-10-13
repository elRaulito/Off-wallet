"""Copy the ROM (and the card readme) to the EverDrive SD card and verify it.

    python tools/deploy_sd.py            # defaults to D:
    python tools/deploy_sd.py E:

Checks the card layout the cart needs -- GBCSYS\\GBCOS.BIN in the root -- and
compares md5 after writing, because a silent bad write on an SD card is worse
than no write at all.
"""
import hashlib, os, shutil, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DRIVE = (sys.argv[1] if len(sys.argv) > 1 else 'D:').rstrip('\\/') + os.sep
FILES = [(os.path.join(ROOT, 'build', 'offwallet.gbc'), 'OFFWALL.GBC'),
         (os.path.join(ROOT, 'dist', 'LEGGIMI.TXT'), 'LEGGIMI.TXT')]


def md5(path):
    return hashlib.md5(open(path, 'rb').read()).hexdigest()


if not os.path.isdir(DRIVE):
    sys.exit('%s is not there -- insert the SD card first' % DRIVE)

fails = 0
for src, name in FILES:
    if not os.path.exists(src):
        print('%-14s MISSING at %s' % (name, src))
        fails += 1
        continue
    dst = os.path.join(DRIVE, name)
    with open(src, 'rb') as i, open(dst, 'wb') as o:
        o.write(i.read())
        o.flush()
        os.fsync(o.fileno())
    ok = md5(dst) == md5(src)
    print('%-14s %s  %s' % (name, md5(dst), 'ok' if ok else 'WRITE MISMATCH'))
    if not ok:
        fails += 1

os_bin = os.path.join(DRIVE, 'GBCSYS', 'GBCOS.BIN')
if os.path.exists(os_bin):
    print('%-14s %s  EverDrive OS in place' % ('GBCSYS/', md5(os_bin)))
else:
    print('GBCSYS/GBCOS.BIN is MISSING -- the cart will show "Memory Error".')
    print('  run: python tools/set_everdrive_os.py 1.06')
    fails += 1

# flush the drive's write cache so the card can be pulled straight away
try:
    import ctypes
    h = ctypes.windll.kernel32.CreateFileW(r'\\.\%s' % DRIVE[:2], 0x40000000,
                                           3, None, 3, 0, None)
    if h != -1:
        ctypes.windll.kernel32.FlushFileBuffers(h)
        ctypes.windll.kernel32.CloseHandle(h)
        print('write cache flushed')
except Exception as e:
    print('could not flush the write cache: %s' % e)

sys.exit(1 if fails else 0)
