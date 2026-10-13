#include "sha256.h"

static const uint32_t K[64] = {
0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))

static void sha256_block(sha256_ctx *c, const uint8_t *p)
{
    uint32_t w[16];
    uint32_t a,b,cc,d,e,f,g,h,t1,t2,s0,s1;
    uint8_t i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|((uint32_t)p[i*4+3]);

    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3];
    e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];

    for (i = 0; i < 64; i++) {
        if (i >= 16) {
            /* rolling message schedule: w[i&15] += ... */
            uint32_t w15 = w[(i+1)&15];
            uint32_t w2  = w[(i+14)&15];
            s0 = ROTR(w15,7) ^ ROTR(w15,18) ^ (w15>>3);
            s1 = ROTR(w2,17) ^ ROTR(w2,19) ^ (w2>>10);
            w[i&15] = w[i&15] + s0 + w[(i+9)&15] + s1;
        }
        s1 = ROTR(e,6) ^ ROTR(e,11) ^ ROTR(e,25);
        t1 = h + s1 + ((e & f) ^ ((~e) & g)) + K[i] + w[i&15];
        s0 = ROTR(a,2) ^ ROTR(a,13) ^ ROTR(a,22);
        t2 = s0 + ((a & b) ^ (a & cc) ^ (b & cc));
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

void sha256_init(sha256_ctx *c)
{
    c->h[0]=0x6a09e667UL; c->h[1]=0xbb67ae85UL; c->h[2]=0x3c6ef372UL; c->h[3]=0xa54ff53aUL;
    c->h[4]=0x510e527fUL; c->h[5]=0x9b05688cUL; c->h[6]=0x1f83d9abUL; c->h[7]=0x5be0cd19UL;
    c->buflen = 0; c->bitlen_hi = 0; c->bitlen_lo = 0;
}

static void add_bits(sha256_ctx *c, uint16_t nbytes)
{
    uint32_t add = (uint32_t)nbytes << 3;
    uint32_t old = c->bitlen_lo;
    c->bitlen_lo = old + add;
    if (c->bitlen_lo < old) c->bitlen_hi++;
}

void sha256_update(sha256_ctx *c, const uint8_t *data, uint16_t len)
{
    uint8_t i;
    add_bits(c, len);
    while (len) {
        if (c->buflen == 0 && len >= 64) {
            sha256_block(c, data);
            data += 64; len -= 64;
        } else {
            uint8_t space = 64 - c->buflen;
            uint16_t n = (len < space) ? len : space;
            for (i = 0; i < (uint8_t)n; i++) c->buf[c->buflen + i] = data[i];
            c->buflen += (uint8_t)n;
            data += n; len -= n;
            if (c->buflen == 64) { sha256_block(c, c->buf); c->buflen = 0; }
        }
    }
}

void sha256_final(sha256_ctx *c, uint8_t out[32])
{
    uint8_t i;
    uint32_t hi = c->bitlen_hi, lo = c->bitlen_lo;
    c->buf[c->buflen++] = 0x80;
    if (c->buflen > 56) {
        while (c->buflen < 64) c->buf[c->buflen++] = 0;
        sha256_block(c, c->buf); c->buflen = 0;
    }
    while (c->buflen < 56) c->buf[c->buflen++] = 0;
    c->buf[56] = (uint8_t)(hi>>24); c->buf[57] = (uint8_t)(hi>>16);
    c->buf[58] = (uint8_t)(hi>>8);  c->buf[59] = (uint8_t)hi;
    c->buf[60] = (uint8_t)(lo>>24); c->buf[61] = (uint8_t)(lo>>16);
    c->buf[62] = (uint8_t)(lo>>8);  c->buf[63] = (uint8_t)lo;
    sha256_block(c, c->buf);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c->h[i]>>24);
        out[i*4+1] = (uint8_t)(c->h[i]>>16);
        out[i*4+2] = (uint8_t)(c->h[i]>>8);
        out[i*4+3] = (uint8_t)(c->h[i]);
    }
}

void sha256(const uint8_t *data, uint16_t len, uint8_t out[32])
{
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, out);
}

void sha256d(const uint8_t *data, uint16_t len, uint8_t out[32])
{
    uint8_t tmp[32];
    sha256(data, len, tmp);
    sha256(tmp, 32, out);
}

void tagged_hash(const char *tag, const uint8_t *msg, uint16_t msglen, uint8_t out[32])
{
    sha256_ctx c;
    uint8_t th[32];
    uint16_t n = 0;
    while (tag[n]) n++;
    sha256((const uint8_t *)tag, n, th);
    sha256_init(&c);
    sha256_update(&c, th, 32);
    sha256_update(&c, th, 32);
    sha256_update(&c, msg, msglen);
    sha256_final(&c, out);
}
