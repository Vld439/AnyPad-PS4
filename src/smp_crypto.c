#include "smp_crypto.h"

#include <string.h>

/* ---- AES-128 (FIPS-197), encryption only --------------------------------- */

static const uint8_t k_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static uint8_t xtime(uint8_t x) { return (uint8_t)(x << 1 ^ (x & 0x80 ? 0x1B : 0)); }

void aes128(const uint8_t key[16], const uint8_t in[16], uint8_t out[16])
{
    uint8_t rk[176], s[16], t[16];
    uint8_t rcon = 1;
    int i, r;

    /* Key expansion: 11 round keys of 16 bytes. */
    memcpy(rk, key, 16);
    for (i = 16; i < 176; i += 4) {
        uint8_t w[4];
        memcpy(w, rk + i - 4, 4);
        if (i % 16 == 0) {
            uint8_t tmp = w[0];
            w[0] = (uint8_t)(k_sbox[w[1]] ^ rcon);
            w[1] = k_sbox[w[2]];
            w[2] = k_sbox[w[3]];
            w[3] = k_sbox[tmp];
            rcon = xtime(rcon);
        }
        rk[i] = rk[i - 16] ^ w[0];
        rk[i + 1] = rk[i - 15] ^ w[1];
        rk[i + 2] = rk[i - 14] ^ w[2];
        rk[i + 3] = rk[i - 13] ^ w[3];
    }

    for (i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
    for (r = 1; r <= 10; r++) {
        /* SubBytes and ShiftRows: state is column-major, s[col * 4 + row]. */
        for (i = 0; i < 16; i++) {
            int row = i % 4, col = i / 4;
            t[i] = k_sbox[s[((col + row) % 4) * 4 + row]];
        }
        if (r < 10) {           /* MixColumns */
            for (i = 0; i < 4; i++) {
                uint8_t *c = t + 4 * i;
                uint8_t a = c[0] ^ c[1] ^ c[2] ^ c[3], c0 = c[0];
                c[0] ^= a ^ xtime(c[0] ^ c[1]);
                c[1] ^= a ^ xtime(c[1] ^ c[2]);
                c[2] ^= a ^ xtime(c[2] ^ c[3]);
                c[3] ^= a ^ xtime(c[3] ^ c0);
            }
        }
        for (i = 0; i < 16; i++) s[i] = t[i] ^ rk[16 * r + i];
    }
    memcpy(out, s, 16);
}

/* ---- AES-CMAC (RFC 4493) ------------------------------------------------- */

static void shift_left(const uint8_t in[16], uint8_t out[16])
{
    int i;

    for (i = 0; i < 15; i++) out[i] = (uint8_t)(in[i] << 1 | in[i + 1] >> 7);
    out[15] = (uint8_t)(in[15] << 1);
}

void aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t mac[16])
{
    static const uint8_t zero[16];
    uint8_t l[16], k1[16], k2[16], x[16], last[16];
    size_t n = (len + 15) / 16, i;
    int complete;

    aes128(key, zero, l);
    shift_left(l, k1);
    if (l[0] & 0x80) k1[15] ^= 0x87;
    shift_left(k1, k2);
    if (k1[0] & 0x80) k2[15] ^= 0x87;

    if (n == 0) {
        n = 1;
        complete = 0;
    } else {
        complete = len % 16 == 0;
    }
    if (complete) {
        for (i = 0; i < 16; i++) last[i] = msg[16 * (n - 1) + i] ^ k1[i];
    } else {
        size_t rest = len - 16 * (n - 1);
        memset(last, 0, 16);
        memcpy(last, msg + 16 * (n - 1), rest);
        last[rest] = 0x80;
        for (i = 0; i < 16; i++) last[i] ^= k2[i];
    }

    memset(x, 0, 16);
    for (i = 0; i + 1 < n; i++) {
        size_t j;
        for (j = 0; j < 16; j++) x[j] ^= msg[16 * i + j];
        aes128(key, x, x);
    }
    for (i = 0; i < 16; i++) x[i] ^= last[i];
    aes128(key, x, mac);
}

/* ---- SMP functions ---------------------------------------------------------- */

void smp_reverse(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;
    uint8_t tmp;

    if (dst == src) {
        for (i = 0; i < n / 2; i++) {
            tmp = dst[i];
            dst[i] = dst[n - 1 - i];
            dst[n - 1 - i] = tmp;
        }
        return;
    }
    for (i = 0; i < n; i++) dst[i] = src[n - 1 - i];
}

void smp_c1(const uint8_t k[16], const uint8_t r[16], const uint8_t preq[7],
            const uint8_t pres[7], uint8_t iat, uint8_t rat,
            const uint8_t ia[6], const uint8_t ra[6], uint8_t out[16])
{
    uint8_t p1[16], p2[16], t[16];
    int i;

    memcpy(p1, pres, 7);                /* p1 = pres || preq || rat' || iat' */
    memcpy(p1 + 7, preq, 7);
    p1[14] = rat & 1;
    p1[15] = iat & 1;
    memset(p2, 0, 4);                   /* p2 = padding || ia || ra */
    memcpy(p2 + 4, ia, 6);
    memcpy(p2 + 10, ra, 6);

    for (i = 0; i < 16; i++) t[i] = r[i] ^ p1[i];
    aes128(k, t, t);
    for (i = 0; i < 16; i++) t[i] ^= p2[i];
    aes128(k, t, out);
}

void smp_s1(const uint8_t k[16], const uint8_t r1[16], const uint8_t r2[16], uint8_t out[16])
{
    uint8_t r[16];

    memcpy(r, r1 + 8, 8);               /* r' = r1' || r2', the low halves */
    memcpy(r + 8, r2 + 8, 8);
    aes128(k, r, out);
}

uint32_t smp_ah(const uint8_t irk[16], uint32_t prand)
{
    uint8_t r[16] = { 0 }, e[16];

    r[13] = (uint8_t)(prand >> 16);
    r[14] = (uint8_t)(prand >> 8);
    r[15] = (uint8_t)prand;
    aes128(irk, r, e);
    return (uint32_t)e[13] << 16 | (uint32_t)e[14] << 8 | e[15];
}

void smp_f4(const uint8_t u[32], const uint8_t v[32], const uint8_t x[16], uint8_t z,
            uint8_t out[16])
{
    uint8_t m[65];

    memcpy(m, u, 32);
    memcpy(m + 32, v, 32);
    m[64] = z;
    aes_cmac(x, m, sizeof m, out);
}

void smp_f5(const uint8_t w[32], const uint8_t n1[16], const uint8_t n2[16],
            const uint8_t a1[7], const uint8_t a2[7], uint8_t mackey[16], uint8_t ltk[16])
{
    static const uint8_t salt[16] = {
        0x6C, 0x88, 0x83, 0x91, 0xAA, 0xF5, 0xA5, 0x38,
        0x60, 0x37, 0x0B, 0xDB, 0x5A, 0x60, 0x83, 0xBE,
    };
    uint8_t t[16], m[53];

    aes_cmac(salt, w, 32, t);
    /* counter || "btle" || N1 || N2 || A1 || A2 || length 256 */
    m[1] = 0x62; m[2] = 0x74; m[3] = 0x6C; m[4] = 0x65;
    memcpy(m + 5, n1, 16);
    memcpy(m + 21, n2, 16);
    memcpy(m + 37, a1, 7);
    memcpy(m + 44, a2, 7);
    m[51] = 0x01;
    m[52] = 0x00;
    m[0] = 0;
    aes_cmac(t, m, sizeof m, mackey);
    m[0] = 1;
    aes_cmac(t, m, sizeof m, ltk);
}

void smp_f6(const uint8_t w[16], const uint8_t n1[16], const uint8_t n2[16],
            const uint8_t r[16], const uint8_t iocap[3], const uint8_t a1[7],
            const uint8_t a2[7], uint8_t out[16])
{
    uint8_t m[65];

    memcpy(m, n1, 16);
    memcpy(m + 16, n2, 16);
    memcpy(m + 32, r, 16);
    memcpy(m + 48, iocap, 3);
    memcpy(m + 51, a1, 7);
    memcpy(m + 58, a2, 7);
    aes_cmac(w, m, sizeof m, out);
}
