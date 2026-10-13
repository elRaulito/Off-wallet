"""Verify the C QR encoder two ways:

  1. decode the module matrix with OpenCV's QR reader and check the text comes
     back -- this is the check that matters, and it is what a phone does;
  2. compare with segno, which agrees module for module whenever both pick the
     same mask pattern (the mask choice is a penalty-rule tie-break and does
     not affect validity).
"""
import subprocess, sys
import numpy as np
import cv2
import segno

out = subprocess.run(['./t_qr.exe'], capture_output=True, text=True).stdout.splitlines()
SIZE = 29
QUIET = 4
SCALE = 8

def to_image(rows):
    n = SIZE + 2 * QUIET
    img = np.full((n, n), 255, dtype=np.uint8)
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch == '1':
                img[y + QUIET, x + QUIET] = 0
    return cv2.resize(img, (n * SCALE, n * SCALE), interpolation=cv2.INTER_NEAREST)

i, cases, fails = 0, 0, 0
det = cv2.QRCodeDetector()
while i < len(out):
    line = out[i]
    if line.startswith('FAIL|'):
        print('encoder refused %r' % line.split('|', 1)[1])
        fails += 1
        i += 1
        continue
    if not line.startswith('CASE|'):
        i += 1
        continue
    _, mask, text = line.split('|', 2)
    mask = int(mask)
    rows = [out[i + 1 + k] for k in range(SIZE)]
    i += 1 + SIZE
    cases += 1

    decoded, _, _ = det.detectAndDecode(to_image(rows))
    ok_decode = (decoded == text)

    q = segno.make(text, mode='alphanumeric', error='l', boost_error=False)
    ref = [''.join('1' if m else '0' for m in row) for row in q.matrix]
    same = (rows == ref)

    status = 'decodes' if ok_decode else 'DOES NOT DECODE'
    extra = 'identical to segno' if same else 'mask %d vs segno %s' % (mask, q.mask)
    print('%-40s %-16s %s' % (text[:40], status, extra))
    if not ok_decode:
        fails += 1
        print('   decoder returned %r' % decoded)

print()
print('%d cases, %d failed to decode' % (cases, fails))
sys.exit(1 if fails else 0)
