#include "crc32.h"
#include "util.h"

static uint32_t g_table[256];
static int g_ready;

static void build_table(void)
{
    uint32_t i, c;
    int k;

    for (i = 0; i < 256; i++) {
        c = i;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_table[i] = c;
    }
    g_ready = 1;
}

static uint32_t update(uint32_t crc, const uint8_t *p, size_t n)
{
    while (n--) crc = g_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc;
}

uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    if (!g_ready) build_table();
    return ~update(0xFFFFFFFFu, data, len);
}

void crc32_seal_output(uint8_t *report, int len)
{
    static const uint8_t hdr = 0xA2;
    uint32_t crc;

    if (!g_ready) build_table();
    crc = update(0xFFFFFFFFu, &hdr, 1);
    crc = ~update(crc, report, (size_t)(len - 4));
    put32(report + len - 4, crc);
}
