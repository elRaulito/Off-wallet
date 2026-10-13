"""Differential check of the C secp256k1 code against Python bignum arithmetic."""
import sys, collections

P  = 2**256 - 2**32 - 977
N  = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
GX = 0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
GY = 0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8

def inv(a, m=P): return pow(a, m - 2, m)

def padd(Pt, Q):
    if Pt is None: return Q
    if Q is None: return Pt
    (x1, y1), (x2, y2) = Pt, Q
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

G = (GX, GY)

vals = collections.defaultdict(dict)
flags = []
for line in open(sys.argv[1]):
    parts = line.split()
    if not parts:
        continue
    if parts[0] in ('ADDSAME', 'DBLSAME'):
        flags.append((parts[0], parts[1], parts[2]))
    elif parts[0] == 'G1':
        vals['G1'][0] = int(parts[1], 16)
    elif len(parts) == 3:
        vals[parts[0]][int(parts[1])] = int(parts[2], 16)

fails = []
def chk(name, i, got, exp):
    if got != exp:
        fails.append('%s[%d]\n  got %064x\n  exp %064x' % (name, i, got, exp))

# field arithmetic
for i in vals['A']:
    a, b = vals['A'][i], vals['B'][i]
    chk('ADD', i, vals['ADD'][i], (a + b) % P)
    chk('SUB', i, vals['SUB'][i], (a - b) % P)
    chk('MUL', i, vals['MUL'][i], a * b % P)
    chk('SQR', i, vals['SQR'][i], a * a % P)
    chk('INV', i, vals['INV'][i], inv(a) if a else 0)
    chk('NEG', i, vals['NEG'][i], (-a) % P)
nfield = len(vals['A'])

# scalars mod n
for i in vals['SA']:
    chk('SCADD', i, vals['SCADD'][i], (vals['SA'][i] + vals['SB'][i]) % N)

# generator
chk('G1X', 0, vals['G1X'][0], GX)
chk('G1Y', 0, vals['G1Y'][0], GY)
if vals['G1'][0] != int('02%064x' % GX, 16):
    fails.append('G1 compressed pubkey mismatch')

# fixed-base multiplication
for i in vals['K']:
    k = vals['K'][i]
    x, y = pmul(k, G)
    chk('MULGX', i, vals['MULGX'][i], x)
    chk('MULGY', i, vals['MULGY'][i], y)
    exp_pub = int('%02x%064x' % (0x03 if y & 1 else 0x02, x), 16)
    chk('PUB', i, vals['PUB'][i], exp_pub)

for kind, idx, res in flags:
    if res != 'OK':
        fails.append('%s %s -> %s' % (kind, idx, res))

print('field op sets: %d (x6 ops), scalar adds: %d, k*G: %d, add/dbl identities: %d'
      % (nfield, len(vals['SA']), len(vals['K']), len(flags)))
if fails:
    print('FAILURES: %d' % len(fails))
    for f in fails[:6]: print(f)
    sys.exit(1)
print('ALL SECP256K1 TESTS PASS')
