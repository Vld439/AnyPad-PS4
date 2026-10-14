/* The LE pairing cryptography against the sample data of the Bluetooth
 * Core specification (Vol 3 Part H, 2.2 and Appendix D) and FIPS-197. */
#include "../src/smp_crypto.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void hex(const char *s, uint8_t *out, size_t n)
{
    size_t i = 0;

    while (*s && i < n) {
        unsigned v;
        if (*s == ' ') { s++; continue; }
        sscanf(s, "%2x", &v);
        out[i++] = (uint8_t)v;
        s += 2;
    }
}

static int same(const uint8_t *a, const char *expect, size_t n)
{
    uint8_t b[64];
    hex(expect, b, n);
    return memcmp(a, b, n) == 0;
}

int main(void)
{
    uint8_t k[16], m[64], out[16], r[16], r2[16], preq[7], pres[7], ia[6], ra[6];
    uint8_t u[32], v[32], w[32], n1[16], n2[16], a1[7], a2[7], mac[16], ltk[16], io[3];

    printf("AES-128 (FIPS-197 C.1)\n");
    hex("000102030405060708090a0b0c0d0e0f", k, 16);
    hex("00112233445566778899aabbccddeeff", m, 16);
    aes128(k, m, out);
    CHECK(same(out, "69c4e0d86a7b0430d8cdb78070b4c55a", 16));

    printf("AES-CMAC (RFC 4493, spec D.1)\n");
    hex("2b7e151628aed2a6abf7158809cf4f3c", k, 16);
    aes_cmac(k, m, 0, out);
    CHECK(same(out, "bb1d6929e95937287fa37d129b756746", 16));
    hex("6bc1bee22e409f96e93d7e117393172a", m, 16);
    aes_cmac(k, m, 16, out);
    CHECK(same(out, "070a16b46b4d4144f79bdd9dd04a287c", 16));
    hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411", m, 40);
    aes_cmac(k, m, 40, out);
    CHECK(same(out, "dfa66747de9ae63030ca32611497c827", 16));
    hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710", m, 64);
    aes_cmac(k, m, 64, out);
    CHECK(same(out, "51f0bebf7e3b9d92fc49741779363cfe", 16));

    printf("c1, s1 (legacy pairing, spec 2.2.3-4)\n");
    memset(k, 0, 16);
    hex("5783D52156AD6F0E6388274EC6702EE0", r, 16);
    hex("07071000000101", preq, 7);
    hex("05000800000302", pres, 7);
    hex("A1A2A3A4A5A6", ia, 6);
    hex("B1B2B3B4B5B6", ra, 6);
    smp_c1(k, r, preq, pres, 1, 0, ia, ra, out);
    CHECK(same(out, "1E1E3FEF878988EAD2A74DC5BEF13B86", 16));
    hex("000F0E0D0C0B0A091122334455667788", r, 16);
    hex("010203040506070899AABBCCDDEEFF00", r2, 16);
    smp_s1(k, r, r2, out);
    CHECK(same(out, "9a1fe1f0e8b0f49b5b4216ae796da062", 16));

    printf("ah (spec D.7)\n");
    hex("ec0234a357c8ad05341010a60a397d9b", k, 16);
    CHECK(smp_ah(k, 0x708194) == 0x0dfbaa);

    printf("f4, f5, f6 (spec D.2-4)\n");
    hex("20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6", u, 32);
    hex("55188b3d32f6bb9a900afcfbeed4e72a59cb9ac2f19d7cfb6b4fdd49f47fc5fd", v, 32);
    hex("d5cb8454d177733effffb2ec712baeab", k, 16);
    smp_f4(u, v, k, 0, out);
    CHECK(same(out, "f2c916f107a9bd1cf1eda1bea974872d", 16));

    hex("ec0234a357c8ad05341010a60a397d9b99796b13b4f866f1868d34f373bfa698", w, 32);
    hex("d5cb8454d177733effffb2ec712baeab", n1, 16);
    hex("a6e8e7cc25a75f6e216583f7ff3dc4cf", n2, 16);
    hex("00561237 37bfce", a1, 7);
    hex("00a71370 2dcfc1", a2, 7);
    smp_f5(w, n1, n2, a1, a2, mac, ltk);
    CHECK(same(mac, "2965f176a1084a02fd3f6a20ce636e20", 16));
    CHECK(same(ltk, "6986791169d7cd23980522b594750a38", 16));

    hex("12a3343bb453bb5408da42d20c2d0fc8", r, 16);
    hex("010102", io, 3);
    smp_f6(mac, n1, n2, r, io, a1, a2, out);
    CHECK(same(out, "e3c473989cd0e8c5d26c0b09da958f61", 16));

    printf(g_fail ? "%d check(s) failed\n" : "all crypto checks passed\n", g_fail);
    return g_fail != 0;
}
