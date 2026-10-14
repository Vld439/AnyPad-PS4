/* The cryptographic functions of LE pairing (Core spec Vol 3 Part H, 2.2).
 *
 * Every value here is in the specification's notation: most significant
 * byte first. Values sent over the air are least significant byte first;
 * the caller reverses them at that boundary.
 */
#ifndef PH_SMP_CRYPTO_H
#define PH_SMP_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/* e(key, plaintext): AES-128. */
void aes128(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

/* AES-CMAC (RFC 4493). */
void aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t mac[16]);

/* LE legacy pairing. preq and pres are the 7-byte pairing request and
 * response commands; ia and ra the initiator's and responder's addresses,
 * iat and rat their types (0 public, 1 random). */
void smp_c1(const uint8_t k[16], const uint8_t r[16], const uint8_t preq[7],
            const uint8_t pres[7], uint8_t iat, uint8_t rat,
            const uint8_t ia[6], const uint8_t ra[6], uint8_t out[16]);
void smp_s1(const uint8_t k[16], const uint8_t r1[16], const uint8_t r2[16], uint8_t out[16]);

/* Resolvable private addresses: the 24-bit hash of prand under an IRK. */
uint32_t smp_ah(const uint8_t irk[16], uint32_t prand);

/* LE Secure Connections. u and v are public key x-coordinates (32 bytes),
 * a1 and a2 an address type byte followed by the address (7 bytes). */
void smp_f4(const uint8_t u[32], const uint8_t v[32], const uint8_t x[16], uint8_t z,
            uint8_t out[16]);
void smp_f5(const uint8_t w[32], const uint8_t n1[16], const uint8_t n2[16],
            const uint8_t a1[7], const uint8_t a2[7], uint8_t mackey[16], uint8_t ltk[16]);
void smp_f6(const uint8_t w[16], const uint8_t n1[16], const uint8_t n2[16],
            const uint8_t r[16], const uint8_t iocap[3], const uint8_t a1[7],
            const uint8_t a2[7], uint8_t out[16]);

/* Byte order swap between the air (LSB first) and the spec (MSB first). */
void smp_reverse(uint8_t *dst, const uint8_t *src, size_t n);

#endif
