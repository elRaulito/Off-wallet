"""Emit the BIP-39 English wordlist as a fixed-stride ROM table."""
import hashlib, io
words = [w.strip() for w in io.open('../../test/vectors/bip39-english.txt',
                                    encoding='utf-8').read().split()]
assert len(words) == 2048, len(words)
# The official list is identified by this SHA-256 (of the newline-joined file)
digest = hashlib.sha256(('\n'.join(words) + '\n').encode()).hexdigest()
assert max(len(w) for w in words) <= 8, 'a word is longer than 8 chars'
assert all(w == w.lower() and w.isalpha() for w in words)
assert words == sorted(words), 'wordlist must be sorted'
print('wordlist ok: 2048 words, max len %d, sha256 %s'
      % (max(len(w) for w in words), digest))

STRIDE = 8
blob = bytearray()
for w in words:
    blob += w.encode() + b'\0' * (STRIDE - len(w))
assert len(blob) == 2048 * STRIDE == 16384

out = ['/* AUTO-GENERATED BIP-39 English wordlist, %d words at %d-byte stride.' % (len(words), STRIDE),
       '   sha256 of the source list: %s' % digest,
       '   Lives in its own ROM bank. */',
       '#ifdef OW_GAMEBOY',
       '#pragma bank 3',
       '#endif',
       '#include <stdint.h>',
       '',
       'const uint8_t BIP39_WORDS[%d] = {' % len(blob)]
for i in range(0, len(blob), 16):
    out.append('  ' + ','.join('0x%02x' % b for b in blob[i:i+16]) + ',')
out.append('};')
io.open('../../src/crypto/bip39_words.c','w',newline='\n').write('\n'.join(out) + '\n')

hdr = ['/* AUTO-GENERATED: BIP-39 English wordlist accessor. */',
       '#ifndef OW_BIP39_WORDS_H',
       '#define OW_BIP39_WORDS_H',
       '#include <stdint.h>',
       '',
       '#define BIP39_STRIDE     %d' % STRIDE,
       '#define BIP39_NWORDS     2048',
       '#define BIP39_WORDS_BANK 3',
       '',
       'extern const uint8_t BIP39_WORDS[%d];' % len(blob),
       '',
       '#ifdef OW_GAMEBOY',
       '#include <gb/gb.h>',
       'static const uint8_t *bip39_words_ptr(uint16_t off)',
       '{',
       '    SWITCH_ROM(BIP39_WORDS_BANK);',
       '    return &BIP39_WORDS[off];',
       '}',
       '#else',
       'static const uint8_t *bip39_words_ptr(uint16_t off)',
       '{',
       '    return &BIP39_WORDS[off];',
       '}',
       '#endif',
       '',
       '#endif']
io.open('../../src/crypto/bip39_words.h','w',newline='\n').write('\n'.join(hdr) + '\n')
print('emitted src/crypto/bip39_words.c (%d bytes of table)' % len(blob))
