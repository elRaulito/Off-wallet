"""Swap the EverDrive GB OS on the SD card.

    python tools/set_everdrive_os.py 1.05      write that version to D:\\GBCSYS
    python tools/set_everdrive_os.py           just list versions and show
                                               which one is on the card

Files come from dist/official-GBC-OS-v*.zip, downloaded from
krikzz.com/pub/support/everdrive-gb/x-series/OS/. The card copy is verified by
reading it back.

Version notes from the official changelist -- read before downgrading:
  1.06  Analogue Pocket fix option (only affects FPGA core v7+)
  1.05  system changes REQUIRED for carts with FPGA core v6 or later
  1.04  cheat engine bugfix
  1.03  save size detection for 128K SRAM
  1.02  FAT bugfix for short LFN save names
  1.01  memory size fix for LSDJ
  1.00  first release
A cart manufactured recently probably has FPGA core v6+, so anything below
1.05 may be missing the system support it needs. Try 1.05 first.
"""
import glob, hashlib, os, re, sys, zipfile, ctypes

DIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'dist')
CARD = 'D:' + os.sep
MEMBER = 'GBCSYS/GBCOS.BIN'


def available():
    out = {}
    for p in sorted(glob.glob(os.path.join(DIST, 'official-GBC-OS-v*.zip'))):
        m = re.search(r'v(\d+\.\d+)\.zip$', p)
        if m:
            data = zipfile.ZipFile(p).read(MEMBER)
            out[m.group(1)] = (p, hashlib.md5(data).hexdigest(), len(data))
    return out


def on_card():
    p = os.path.join(CARD, 'GBCSYS', 'GBCOS.BIN')
    if not os.path.exists(p):
        return None
    return hashlib.md5(open(p, 'rb').read()).hexdigest()


def flush():
    h = ctypes.windll.kernel32.CreateFileW(r'\\.\D:', 0x40000000, 3, None, 3, 0, None)
    if h != -1:
        ctypes.windll.kernel32.FlushFileBuffers(h)
        ctypes.windll.kernel32.CloseHandle(h)


def main():
    vers = available()
    if not vers:
        sys.exit('no OS zips in dist/ -- download them from krikzz.com first')
    cur = on_card()

    if len(sys.argv) < 2:
        print('%-8s %-34s %s' % ('version', 'md5', 'on card'))
        for v in sorted(vers):
            print('%-8s %-34s %s' % (v, vers[v][1], 'YES' if vers[v][1] == cur else ''))
        if cur and cur not in [x[1] for x in vers.values()]:
            print('\ncard holds an unrecognised GBCOS.BIN (md5 %s)' % cur)
        if not cur:
            print('\nno GBCSYS\\GBCOS.BIN on the card')
        return

    want = sys.argv[1].lstrip('vV')
    if want not in vers:
        sys.exit('unknown version %r; have: %s' % (want, ', '.join(sorted(vers))))

    zp, md5, size = vers[want]
    data = zipfile.ZipFile(zp).read(MEMBER)
    dest_dir = os.path.join(CARD, 'GBCSYS')
    os.makedirs(dest_dir, exist_ok=True)
    dest = os.path.join(dest_dir, 'GBCOS.BIN')
    with open(dest, 'wb') as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    flush()

    back = hashlib.md5(open(dest, 'rb').read()).hexdigest()
    print('wrote GBC-OS v%s to %s (%d bytes)' % (want, dest, size))
    print('expected md5 %s' % md5)
    print('read back    %s  %s' % (back, 'ok' if back == md5 else 'MISMATCH'))
    if back != md5:
        sys.exit(1)


if __name__ == '__main__':
    main()
