/* The endpoint picker against the PS5's real descriptor, and event reassembly. */
#include "../src/evstream.h"
#include "../src/usb_desc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int load_fixture(const char *path, uint8_t *out, int max)
{
    char line[256];
    FILE *f = fopen(path, "r");
    int n = 0;

    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        if (line[0] == '#') continue;
        while (*p) {
            unsigned v;
            while (*p == ' ' || *p == '\n') p++;
            if (!*p) break;
            if (sscanf(p, "%2x", &v) != 1) break;
            if (n < max) out[n++] = (uint8_t)v;
            p += 2;
        }
    }
    fclose(f);
    return n;
}

static void test_descriptor(void)
{
    uint8_t cfg[512];
    hci_function f[HCI_FUNCTIONS_MAX];
    int len = load_fixture("tests/fixtures/ps5_fw1001_config_descriptor.hex", cfg, sizeof cfg), n, i;

    printf("endpoints from the PS5 fat / firmware 10.01 descriptor\n");
    CHECK(len == 474);
    n = usb_find_hci_functions(cfg, len, f);
    CHECK(n == 2);                                  /* two HCI functions, voice interfaces skipped */
    if (n == 2) {
        /* function A: interface 0 */
        CHECK(f[0].iface == 0);
        CHECK(f[0].ep_events == 0x81 && f[0].mps_events == 16);     /* interrupt IN */
        CHECK(f[0].ep_acl_in == 0x82 && f[0].mps_acl_in == 512);    /* bulk IN */
        CHECK(f[0].ep_acl_out == 0x01 && f[0].mps_acl_out == 512);  /* first bulk OUT */
        CHECK(f[0].n_other == 2 && f[0].other[0] == 0x02 && f[0].other[1] == 0x8F);
        /* function B: interface 3 */
        CHECK(f[1].iface == 3);
        CHECK(f[1].ep_events == 0x8C && f[1].ep_acl_in == 0x8D && f[1].ep_acl_out == 0x0B);
    }

    printf("ACL out: the spare OUT numbered like the IN on the MediaTek chip, the first otherwise\n");
    if (n == 2) CHECK(usb_spare_acl_out(&f[0]) == 0x02);     /* 0x82 in -> 0x02, as AnyPad always used */
    {
        hci_function m;
        memset(&m, 0, sizeof m);
        m.ep_events = 0x81; m.ep_acl_in = 0x82; m.ep_acl_out = 0x02;     /* first OUT already pairs */
        m.n_other = 1; m.other[0] = 0x01;
        CHECK(usb_spare_acl_out(&m) == 0);
        m.ep_acl_out = 0x01; m.other[0] = 0x03;                          /* a spare that does not pair */
        CHECK(usb_spare_acl_out(&m) == 0);
        m.n_other = 0;                                                   /* no spare at all */
        CHECK(usb_spare_acl_out(&m) == 0);
        CHECK(usb_spare_acl_out(NULL) == 0);
    }

    printf("a truncated or malformed descriptor never reads past its end\n");
    for (i = 0; i <= len; i += 7) usb_find_hci_functions(cfg, i, f);
    cfg[30] = 0;                                    /* a zero-length descriptor: stop */
    CHECK(usb_find_hci_functions(cfg, len, f) <= HCI_FUNCTIONS_MAX);
    CHECK(usb_find_hci_functions(cfg, 0, f) == 0);
}

typedef struct { uint8_t ev[8][300]; int len[8]; int n; } sink;

static void emit(void *ctx, const uint8_t *e, int len)
{
    sink *s = ctx;
    if (s->n < 8) {
        memcpy(s->ev[s->n], e, (size_t)len);
        s->len[s->n++] = len;
    }
}

static void test_evstream(void)
{
    evstream es;
    sink s;
    uint8_t big[257], two[40];
    int i;

    printf("event reassembly\n");
    evstream_init(&es);
    memset(&s, 0, sizeof s);

    /* one short event in one piece */
    {
        static const uint8_t cc[] = { 0x0E, 0x0B, 0x01, 0x05, 0x10, 0x00, 0xFD, 0x03, 0xF0, 0x08, 0x00, 0x08, 0x00 };
        evstream_feed(&es, cc, 13, 1000, emit, &s);
        CHECK(s.n == 1 && s.len[0] == 13 && memcmp(s.ev[0], cc, 13) == 0);
    }

    /* a 255+2 byte event in 16-byte pieces */
    s.n = 0;
    big[0] = 0x2F;
    big[1] = 255;
    for (i = 2; i < 257; i++) big[i] = (uint8_t)i;
    for (i = 0; i < 257; i += 16) evstream_feed(&es, big + i, 257 - i < 16 ? 257 - i : 16, 2000 + i, emit, &s);
    CHECK(s.n == 1 && s.len[0] == 257 && memcmp(s.ev[0], big, 257) == 0);

    /* two events in one piece, and one split across two */
    s.n = 0;
    memset(two, 0, sizeof two);
    two[0] = 0x0F; two[1] = 4;                      /* command status: 6 bytes */
    two[8] = 0x13; two[9] = 5;                      /* number of completed packets: 7 bytes */
    two[6] = 0x0E; two[7] = 0;                      /* a bare 2-byte event between */
    evstream_feed(&es, two, 8, 3000, emit, &s);
    CHECK(s.n == 2 && s.len[0] == 6 && s.len[1] == 2);
    evstream_feed(&es, two + 8, 3, 3001, emit, &s);
    CHECK(s.n == 2);                                /* half an event waits */
    evstream_feed(&es, two + 11, 4, 3002, emit, &s);
    CHECK(s.n == 3 && s.len[2] == 7);

    /* a lost piece: the half event is given up on, the next one is fine */
    s.n = 0;
    evstream_feed(&es, big, 16, 4000, emit, &s);    /* the start of a long event */
    CHECK(s.n == 0);
    {
        static const uint8_t cs[] = { 0x0F, 0x04, 0x00, 0x01, 0x05, 0x04 };
        evstream_feed(&es, cs, 6, 4500, emit, &s);  /* 500 ms later: the old one is dropped */
        CHECK(es.dropped == 1);
        CHECK(s.n == 1 && s.len[0] == 6 && memcmp(s.ev[0], cs, 6) == 0);
    }

    /* noise cannot overrun the buffer */
    for (i = 0; i < 1000; i++) {
        uint8_t junk[16];
        memset(junk, 0xFF, sizeof junk);
        evstream_feed(&es, junk, 16, 5000 + i, emit, &s);
    }
    CHECK(es.len < (int)sizeof es.buf);
}

int main(void)
{
    test_descriptor();
    test_evstream();
    printf(g_fail ? "%d check(s) failed\n" : "all usb checks passed\n", g_fail);
    return g_fail != 0;
}
