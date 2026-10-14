/* Byte order and the clock, shared by every module. */
#ifndef PH_UTIL_H
#define PH_UTIL_H

#include <stdint.h>

/* Milliseconds on the monotonic clock. Tests replace the clock through
 * ph_clock_hook to run timers without waiting. */
long now_ms(void);
extern long (*ph_clock_hook)(void);

/* HCI, L2CAP and HID are little-endian; SDP is big-endian. */
static inline unsigned le16(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static inline unsigned be16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static inline uint32_t be32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static inline int16_t les16(const unsigned char *p) { return (int16_t)le16(p); }

static inline void put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)(v >> 8);
}

static inline void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

#endif
