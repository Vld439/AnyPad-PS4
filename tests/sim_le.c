#include "sim_le.h"
#include "../src/smp_crypto.h"
#include "../src/util.h"

#include <stdlib.h>
#include <string.h>

#define QLEN    256
#define PKT     600
#define HANDLE  0x0050

typedef struct {
    unsigned char d[QLEN][PKT];
    int len[QLEN];
    int head, count;
} queue;

/* The GATT database, handle by handle. */
enum {
    H_DIS = 1, H_PNP_DECL, H_PNP,
    H_HID = 16, H_MAP_DECL, H_MAP, H_IN_DECL, H_IN, H_IN_CCCD, H_IN_REF,
    H_OUT_DECL, H_OUT, H_OUT_REF, H_INFO_DECL, H_INFO,
    H_CTL_DECL, H_CTL,          /* only on a pad that needs waking */
    H_LAST = H_CTL,
};

struct sim_le {
    sim_le_pad pad;
    queue ev, acl;
    uint8_t host_addr[6];
    uint8_t cur_addr[6];        /* the address advertised now */
    uint8_t cur_type;
    int powered, scanning, connected, encrypted;
    long t_adv, t_report;

    /* pairing, as the responder; MSB first like the spec */
    int pairings, paired, sc;
    uint8_t preq[7], pres[7];   /* as sent */
    uint8_t mconfirm[16], na[16], nb[16], mackey[16], ltk[16];
    uint8_t host_pk[64];
    int expect_lsb_valid;
    uint8_t expect_ltk[16];     /* the key encryption must use, LSB first */
    uint8_t expect_rand[8];
    uint16_t expect_ediv;
    /* the bond, once made */
    int bonded;
    uint8_t bond_ltk[16], bond_rand[8];
    uint16_t bond_ediv;

    uint16_t mtu, cccd;
    int outputs, blob_reads;
    int identity;               /* 1: IRK received, 2: and the address */
    int creates, cancels;
    int awake;
    uint8_t last_out[64];
    int last_out_len;
    unsigned drop_state;
    const char *error;
    int ssp, scan;
};

static long g_clock = 5000;
long sim_le_now(void) { return g_clock; }

/* The controller's P-256 maths, stood in for: whatever public keys go
 * across, both sides get this same DHKey. */
static const uint8_t k_host_pk[64] = {
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x02,
};
static const uint8_t k_pad_pk[64] = {
    0x22, 0x21, 0x22, 0x23, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x03,
    0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
    0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x04,
};
static const uint8_t k_dhkey[32] = {        /* least significant byte first */
    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42,
    0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52,
};
static const uint8_t k_host_addr[6] = { 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1 };

/* ---- queues and lossy delivery ----------------------------------------------- */

static void q_put(queue *q, const unsigned char *p, int n)
{
    int slot;

    if (q->count == QLEN) return;
    slot = (q->head + q->count) % QLEN;
    memcpy(q->d[slot], p, (size_t)n);
    q->len[slot] = n;
    q->count++;
}

static int q_get(queue *q, unsigned char *out, int max)
{
    int n;

    if (!q->count) return 0;
    n = q->len[q->head] < max ? q->len[q->head] : max;
    memcpy(out, q->d[q->head], (size_t)n);
    q->head = (q->head + 1) % QLEN;
    q->count--;
    return n;
}

static int lose(sim_le_t *s)
{
    s->drop_state = s->drop_state * 1103515245u + 12345u;
    return s->pad.drop_every && (s->drop_state >> 16) % (unsigned)s->pad.drop_every == 0;
}

/* ---- controller side ------------------------------------------------------ */

static void event(sim_le_t *s, unsigned char code, const unsigned char *p, int n)
{
    unsigned char e[260];

    e[0] = code;
    e[1] = (unsigned char)n;
    memcpy(e + 2, p, (size_t)n);
    q_put(&s->ev, e, 2 + n);
}

static void cmd_complete(sim_le_t *s, unsigned op, const unsigned char *ret, int n)
{
    unsigned char p[64];

    p[0] = 1;
    put16(p + 1, op);
    memcpy(p + 3, ret, (size_t)n);
    event(s, 0x0E, p, 3 + n);
}

static void cmd_status(sim_le_t *s, unsigned op, unsigned char status)
{
    unsigned char p[4];

    p[0] = status;
    p[1] = 1;
    put16(p + 2, op);
    event(s, 0x0F, p, 4);
}

static void meta(sim_le_t *s, const unsigned char *p, int n) { event(s, 0x3E, p, n); }

static void enc_change(sim_le_t *s, unsigned char status)
{
    unsigned char p[4];

    p[0] = status;
    put16(p + 1, HANDLE);
    p[3] = status == 0;
    event(s, 0x08, p, 4);
}

static void new_address(sim_le_t *s)
{
    if (s->pad.use_rpa) {
        uint8_t irk[16];
        uint32_t prand = ((uint32_t)rand() & 0x3FFFFF) | 0x400000;   /* top bits 01 */
        uint32_t hash;

        smp_reverse(irk, s->pad.irk, 16);
        hash = smp_ah(irk, prand);
        s->cur_addr[0] = (uint8_t)hash;
        s->cur_addr[1] = (uint8_t)(hash >> 8);
        s->cur_addr[2] = (uint8_t)(hash >> 16);
        s->cur_addr[3] = (uint8_t)prand;
        s->cur_addr[4] = (uint8_t)(prand >> 8);
        s->cur_addr[5] = (uint8_t)(prand >> 16);
        s->cur_type = 1;
    } else {
        memcpy(s->cur_addr, s->pad.addr, 6);
        s->cur_type = 0;
    }
}

/* ---- the pad's L2CAP: SMP and ATT ------------------------------------------ */

static void acl_to_host(sim_le_t *s, unsigned cid, const unsigned char *d, int n)
{
    unsigned char pkt[PKT];

    put16(pkt, HANDLE | 0x2000);
    put16(pkt + 2, (unsigned)(4 + n));
    put16(pkt + 4, (unsigned)n);
    put16(pkt + 6, cid);
    memcpy(pkt + 8, d, (size_t)n);
    q_put(&s->acl, pkt, 8 + n);
}

static void smp_out(sim_le_t *s, uint8_t code, const uint8_t *v, int n)
{
    uint8_t p[72];

    p[0] = code;
    memcpy(p + 1, v, (size_t)n);
    acl_to_host(s, 6, p, n + 1);
}

static void smp_out_rev(sim_le_t *s, uint8_t code, const uint8_t *msb, int n)
{
    uint8_t v[64];

    smp_reverse(v, msb, (size_t)n);
    smp_out(s, code, v, n);
}

static void fail(sim_le_t *s, const char *why, uint8_t reason)
{
    s->error = why;
    smp_out(s, 0x05, &reason, 1);
}

static void legacy_c1(sim_le_t *s, const uint8_t r[16], uint8_t out[16])
{
    static const uint8_t tk[16];
    uint8_t preq[7], pres[7], ia[6], ra[6];

    smp_reverse(preq, s->preq, 7);
    smp_reverse(pres, s->pres, 7);
    smp_reverse(ia, s->host_addr, 6);
    smp_reverse(ra, s->cur_addr, 6);
    smp_c1(tk, r, preq, pres, 0, s->cur_type, ia, ra, out);
}

static void sc_addrs(sim_le_t *s, uint8_t a[7], uint8_t b[7])
{
    a[0] = 0;
    smp_reverse(a + 1, s->host_addr, 6);
    b[0] = s->cur_type;
    smp_reverse(b + 1, s->cur_addr, 6);
}

static void on_smp(sim_le_t *s, const uint8_t *d, int n)
{
    uint8_t v[16];

    if (d[0] == 0x08 && n >= 17) {      /* the host's identity key */
        s->identity = 1;
        return;
    }
    if (d[0] == 0x09) {                 /* and its public address, after it */
        if (n >= 8 && s->identity == 1 && d[1] == 0 && memcmp(d + 2, s->host_addr, 6) == 0) s->identity = 2;
        else s->error = "host identity handed over wrongly";
        return;
    }
    switch (d[0]) {
    case 0x01:                          /* pairing request */
        if (n < 7) return;
        s->pairings++;
        memcpy(s->preq, d, 7);
        s->sc = s->pad.sc && (d[3] & 0x08);
        s->pres[0] = 0x02;
        s->pres[1] = 0x03;
        s->pres[2] = 0x00;
        s->pres[3] = (uint8_t)(0x01 | (s->pad.sc ? 0x08 : 0));
        s->pres[4] = 16;
        s->pres[5] = s->pad.wants_identity ? (uint8_t)(d[5] & 0x02) : 0x00;
        s->identity = 0;
        s->pres[6] = d[6] & 0x03;
        smp_out(s, 0x02, s->pres + 1, 6);
        memset(s->nb, 0x5B, 16);
        s->nb[15] = (uint8_t)s->pairings;
        break;

    case 0x0C:                          /* host public key */
        if (!s->sc || n < 65) return;
        memcpy(s->host_pk, d + 1, 64);
        if (memcmp(s->host_pk, k_host_pk, 64) != 0) s->error = "host sent a key not from the controller";
        smp_out(s, 0x0C, k_pad_pk, 64);
        {
            uint8_t pbx[32], pax[32], cb[16];
            smp_reverse(pbx, k_pad_pk, 32);
            smp_reverse(pax, s->host_pk, 32);
            smp_f4(pbx, pax, s->nb, 0, cb);
            smp_out_rev(s, 0x03, cb, 16);
        }
        break;

    case 0x03:                          /* host confirm (legacy) */
        if (s->sc || n < 17) return;
        smp_reverse(s->mconfirm, d + 1, 16);
        legacy_c1(s, s->nb, v);
        smp_out_rev(s, 0x03, v, 16);
        break;

    case 0x04:                          /* host random */
        if (n < 17) return;
        smp_reverse(s->na, d + 1, 16);
        if (!s->sc) {
            static const uint8_t tk[16];
            uint8_t stk[16];
            legacy_c1(s, s->na, v);
            if (memcmp(v, s->mconfirm, 16) != 0) {
                fail(s, "host confirm value wrong", 0x04);
                return;
            }
            smp_out_rev(s, 0x04, s->nb, 16);
            smp_s1(tk, s->nb, s->na, stk);
            smp_reverse(s->expect_ltk, stk, 16);
        } else {
            uint8_t w[32], a[7], b[7];
            smp_out_rev(s, 0x04, s->nb, 16);
            smp_reverse(w, k_dhkey, 32);
            sc_addrs(s, a, b);
            smp_f5(w, s->na, s->nb, a, b, s->mackey, s->ltk);
            smp_reverse(s->expect_ltk, s->ltk, 16);
        }
        memset(s->expect_rand, 0, 8);
        s->expect_ediv = 0;
        s->expect_lsb_valid = 1;
        break;

    case 0x0D:                          /* host DHKey check */
        if (!s->sc || n < 17) return;
        {
            uint8_t a[7], b[7], io[3], r[16] = { 0 }, ea[16], eb[16];
            sc_addrs(s, a, b);
            io[0] = s->preq[3];
            io[1] = s->preq[2];
            io[2] = s->preq[1];
            smp_f6(s->mackey, s->na, s->nb, r, io, a, b, ea);
            smp_reverse(v, d + 1, 16);
            if (memcmp(v, ea, 16) != 0) {
                fail(s, "host DHKey check wrong", 0x0B);
                return;
            }
            io[0] = s->pres[3];
            io[1] = s->pres[2];
            io[2] = s->pres[1];
            smp_f6(s->mackey, s->nb, s->na, r, io, b, a, eb);
            smp_out_rev(s, 0x0D, eb, 16);
        }
        break;

    case 0x05:
        s->error = "host aborted pairing";
        break;
    default:
        break;
    }
}

/* Hands out the keys after pairing, as the pad would, and keeps the bond. */
static void distribute_keys(sim_le_t *s)
{
    uint8_t v[7];

    if (!s->sc && (s->pres[6] & 0x01)) {
        int i;
        for (i = 0; i < 16; i++) s->bond_ltk[i] = (uint8_t)(0xA0 + i);
        for (i = 0; i < 8; i++) s->bond_rand[i] = (uint8_t)(0x70 + i);
        s->bond_ediv = 0x4321;
        smp_out(s, 0x06, s->bond_ltk, 16);
        put16(v, s->bond_ediv);
        {
            uint8_t m[10];
            put16(m, s->bond_ediv);
            memcpy(m + 2, s->bond_rand, 8);
            smp_out(s, 0x07, m, 10);
        }
    } else {
        memcpy(s->bond_ltk, s->expect_ltk, 16);
        memset(s->bond_rand, 0, 8);
        s->bond_ediv = 0;
    }
    if (s->pres[6] & 0x02) {
        smp_out(s, 0x08, s->pad.irk, 16);
        v[0] = 0;
        memcpy(v + 1, s->pad.addr, 6);
        smp_out(s, 0x09, v, 7);
    }
    s->bonded = 1;
    s->paired = 1;
}

/* ---- GATT server ---------------------------------------------------------- */

static int attr_value(sim_le_t *s, uint16_t h, uint8_t *out, uint16_t *type)
{
    uint8_t *p = out;

    switch (h) {
    case H_DIS:  *type = 0x2800; put16(p, 0x180A); return 2;
    case H_HID:  *type = 0x2800; put16(p, 0x1812); return 2;
    case H_PNP_DECL: *type = 0x2803; p[0] = 0x02; put16(p + 1, H_PNP); put16(p + 3, 0x2A50); return 5;
    case H_PNP:
        *type = 0x2A50;
        p[0] = 0x02;
        put16(p + 1, s->pad.vid);
        put16(p + 3, s->pad.pid);
        put16(p + 5, 0x0001);
        return 7;
    case H_MAP_DECL: *type = 0x2803; p[0] = 0x02; put16(p + 1, H_MAP); put16(p + 3, 0x2A4B); return 5;
    case H_MAP:  *type = 0x2A4B; memcpy(p, s->pad.map, (size_t)s->pad.map_len); return s->pad.map_len;
    case H_IN_DECL: *type = 0x2803; p[0] = 0x12; put16(p + 1, H_IN); put16(p + 3, 0x2A4D); return 5;
    case H_IN:   *type = 0x2A4D; memcpy(p, s->pad.report, (size_t)s->pad.report_len); return s->pad.report_len;
    case H_IN_CCCD: *type = 0x2902; put16(p, s->cccd); return 2;
    case H_IN_REF: *type = 0x2908; p[0] = s->pad.rid_in; p[1] = 1; return 2;
    case H_OUT_DECL: *type = 0x2803; p[0] = 0x0E; put16(p + 1, H_OUT); put16(p + 3, 0x2A4D); return 5;
    case H_OUT:  *type = 0x2A4D; memset(p, 0, 8); return 8;
    case H_OUT_REF: *type = 0x2908; p[0] = s->pad.rid_out; p[1] = 2; return 2;
    case H_INFO_DECL: *type = 0x2803; p[0] = 0x02; put16(p + 1, H_INFO); put16(p + 3, 0x2A4A); return 5;
    case H_INFO: *type = 0x2A4A; p[0] = 0x11; p[1] = 0x01; p[2] = 0; p[3] = 0x02; return 4;
    case H_CTL_DECL:
        if (!s->pad.needs_wake) return -1;
        *type = 0x2803; p[0] = 0x04; put16(p + 1, H_CTL); put16(p + 3, 0x2A4C); return 5;
    case H_CTL:
        if (!s->pad.needs_wake) return -1;
        *type = 0x2A4C; p[0] = (uint8_t)s->awake; return 1;
    default: return -1;
    }
}

static int attr_exists(sim_le_t *s, uint16_t h)
{
    uint8_t v[600];
    uint16_t t;

    if (s->pad.no_pnp && (h == H_DIS || h == H_PNP_DECL || h == H_PNP)) return 0;
    return attr_value(s, h, v, &t) >= 0;
}

static void att_error(sim_le_t *s, uint8_t op, uint16_t h, uint8_t code)
{
    uint8_t r[5];

    r[0] = 0x01;
    r[1] = op;
    put16(r + 2, h);
    r[4] = code;
    acl_to_host(s, 4, r, 5);
}

static void on_att(sim_le_t *s, const uint8_t *d, int n)
{
    uint8_t r[600], v[600];
    uint16_t t, h, start, end;
    int k, len, m = s->mtu;

    switch (d[0]) {
    case 0x02:                          /* exchange MTU */
        r[0] = 0x03;
        put16(r + 1, s->pad.mtu);
        acl_to_host(s, 4, r, 3);
        s->mtu = le16(d + 1) < s->pad.mtu ? (uint16_t)le16(d + 1) : s->pad.mtu;
        break;

    case 0x06:                          /* find by type value: services */
        start = (uint16_t)le16(d + 1);
        end = (uint16_t)le16(d + 3);
        if (le16(d + 5) == 0x2800 && le16(d + 7) == 0x1812 && start <= H_HID && end >= H_HID) {
            r[0] = 0x07;
            put16(r + 1, H_HID);
            put16(r + 3, s->pad.needs_wake ? H_CTL : H_INFO);
            acl_to_host(s, 4, r, 5);
        } else {
            att_error(s, 0x06, start, 0x0A);
        }
        break;

    case 0x08:                          /* read by type */
        start = (uint16_t)le16(d + 1);
        end = (uint16_t)le16(d + 3);
        r[0] = 0x09;
        r[1] = 0;
        k = 2;
        for (h = start; h <= end && h <= H_LAST; h++) {
            if (!attr_exists(s, h)) continue;
            len = attr_value(s, h, v, &t);
            if (t != le16(d + 5)) continue;
            if (h >= H_HID && !s->encrypted) {
                att_error(s, 0x08, h, 0x05);
                return;
            }
            if (r[1] && r[1] != 2 + len) break;
            if (k + 2 + len > m) break;
            r[1] = (uint8_t)(2 + len);
            put16(r + k, h);
            memcpy(r + k + 2, v, (size_t)len);
            k += 2 + len;
        }
        if (k == 2) att_error(s, 0x08, start, 0x0A);
        else acl_to_host(s, 4, r, k);
        break;

    case 0x04:                          /* find information */
        start = (uint16_t)le16(d + 1);
        end = (uint16_t)le16(d + 3);
        r[0] = 0x05;
        r[1] = 1;
        k = 2;
        for (h = start; h <= end && h <= H_LAST && k + 4 <= m; h++) {
            if (!attr_exists(s, h)) continue;
            attr_value(s, h, v, &t);
            put16(r + k, h);
            put16(r + k + 2, t);
            k += 4;
        }
        if (k == 2) att_error(s, 0x04, start, 0x0A);
        else acl_to_host(s, 4, r, k);
        break;

    case 0x0A:                          /* read */
    case 0x0C:                          /* read blob */
        h = (uint16_t)le16(d + 1);
        if (!attr_exists(s, h)) {
            att_error(s, d[0], h, 0x01);
            break;
        }
        if (h >= H_HID && !s->encrypted) {
            att_error(s, d[0], h, 0x05);
            break;
        }
        len = attr_value(s, h, v, &t);
        {
            int off = d[0] == 0x0C ? (int)le16(d + 3) : 0;
            int chunk = len - off < m - 1 ? len - off : m - 1;
            if (d[0] == 0x0C) s->blob_reads++;
            if (chunk < 0) chunk = 0;
            r[0] = (uint8_t)(d[0] + 1);
            memcpy(r + 1, v + off, (size_t)chunk);
            acl_to_host(s, 4, r, 1 + chunk);
        }
        break;

    case 0x12:                          /* write request */
        h = (uint16_t)le16(d + 1);
        if (h == H_IN_CCCD && n >= 5) s->cccd = (uint16_t)le16(d + 3);
        r[0] = 0x13;
        acl_to_host(s, 4, r, 1);
        break;

    case 0x52:                          /* write command */
        if (le16(d + 1) == H_CTL && s->pad.needs_wake && n >= 4) s->awake = d[3] == 0x01;
        if (le16(d + 1) == H_OUT) {
            s->outputs++;
            s->last_out_len = n - 3 < 64 ? n - 3 : 64;
            memcpy(s->last_out, d + 3, (size_t)s->last_out_len);
        }
        break;

    default:
        if (!(d[0] & 1)) att_error(s, d[0], 0, 0x06);
        break;
    }
}

/* ---- hci_ops --------------------------------------------------------------- */

static void disconnect(sim_le_t *s, unsigned char reason)
{
    unsigned char p[4];

    if (!s->connected) return;
    p[0] = 0;
    put16(p + 1, HANDLE);
    p[3] = reason;
    event(s, 0x05, p, 4);
    s->connected = s->encrypted = 0;
    s->expect_lsb_valid = 0;
    s->cccd = 0;
    s->awake = 0;                       /* suspend is per connection */
}

static int op_cmd(void *ctx, unsigned op, const void *params, int plen)
{
    sim_le_t *s = ctx;
    const unsigned char *p = params;
    unsigned char r[80];

    (void)plen;
    switch (op) {
    case 0x1005:
        memset(r, 0, 8);
        put16(r + 1, 1021);
        r[3] = 64;
        put16(r + 4, 8);
        cmd_complete(s, op, r, 8);
        break;
    case 0x0C55:
        r[0] = 0;
        r[1] = (unsigned char)s->ssp;
        cmd_complete(s, op, r, 2);
        break;
    case 0x0C56:
        s->ssp = p[0];
        r[0] = 0;
        cmd_complete(s, op, r, 1);
        break;
    case 0x0C19:
        r[0] = 0;
        r[1] = (unsigned char)s->scan;
        cmd_complete(s, op, r, 2);
        break;
    case 0x0C1A:
        s->scan = p[0];
        r[0] = 0;
        cmd_complete(s, op, r, 1);
        break;
    case 0x1009:
        r[0] = 0;
        memcpy(r + 1, k_host_addr, 6);
        cmd_complete(s, op, r, 7);
        break;
    case 0x2002:
        r[0] = 0;
        put16(r + 1, 251);
        r[3] = 6;
        cmd_complete(s, op, r, 4);
        break;
    case 0x200C:
        s->scanning = p[0];
        r[0] = 0;
        cmd_complete(s, op, r, 1);
        break;
    case 0x200D:                        /* create connection */
        s->creates++;
        if (s->pad.busy > 0) {          /* one attempt at a time */
            cmd_status(s, op, 0x0C);
            break;
        }
        cmd_status(s, op, 0);
        if (s->powered && !s->connected && p[5] == s->cur_type && memcmp(p + 6, s->cur_addr, 6) == 0) {
            r[0] = 0x01;
            r[1] = 0;
            put16(r + 2, HANDLE);
            r[4] = 0;
            r[5] = s->cur_type;
            memcpy(r + 6, s->cur_addr, 6);
            put16(r + 12, 12);
            put16(r + 14, 0);
            put16(r + 16, 200);
            r[18] = 0;
            meta(s, r, 19);
            s->connected = 1;
            s->mtu = 23;
        }
        break;
    case 0x200E:                        /* create connection cancel */
        s->cancels++;
        if (s->pad.busy <= 0) {         /* nothing pending */
            r[0] = 0x0C;
            cmd_complete(s, op, r, 1);
            break;
        }
        r[0] = 0;
        cmd_complete(s, op, r, 1);
        memset(r, 0, 19);
        r[0] = 0x01;
        r[1] = 0x02;                    /* the cancelled attempt ends */
        memset(r + 6, 0xEE, 6);         /* someone else's device */
        meta(s, r, 19);
        s->pad.busy--;
        break;
    case 0x2019:                        /* start encryption */
        cmd_status(s, op, 0);
        if (s->expect_lsb_valid && memcmp(p + 12, s->expect_ltk, 16) == 0 &&
            le16(p + 10) == s->expect_ediv && memcmp(p + 2, s->expect_rand, 8) == 0) {
            s->encrypted = 1;
            s->expect_lsb_valid = 0;
            enc_change(s, 0);
            distribute_keys(s);
        } else if (s->bonded && memcmp(p + 12, s->bond_ltk, 16) == 0 &&
                   le16(p + 10) == s->bond_ediv && memcmp(p + 2, s->bond_rand, 8) == 0) {
            s->encrypted = 1;
            enc_change(s, 0);
        } else {
            s->error = "encryption with the wrong key";
            enc_change(s, 0x06);
        }
        break;
    case 0x2025:
        cmd_status(s, op, 0);
        r[0] = 0x08;
        r[1] = 0;
        memcpy(r + 2, k_host_pk, 64);
        meta(s, r, 66);
        break;
    case 0x2026:
        cmd_status(s, op, 0);
        r[0] = 0x09;
        r[1] = 0;
        memcpy(r + 2, k_dhkey, 32);
        meta(s, r, 34);
        break;
    case 0x0406:
        cmd_status(s, op, 0);
        disconnect(s, 0x16);
        break;
    case 0x2013:
        cmd_status(s, op, 0);
        break;
    default:
        r[0] = 0;
        cmd_complete(s, op, r, 1);
        break;
    }
    return 1;
}

static int op_acl_send(void *ctx, const unsigned char *pkt, int len)
{
    sim_le_t *s = ctx;
    unsigned char ncp[5];
    int n;

    if (len < 8 || !s->connected) return 1;
    ncp[0] = 1;
    put16(ncp + 1, HANDLE);
    put16(ncp + 3, 1);
    event(s, 0x13, ncp, 5);
    n = (int)le16(pkt + 4);
    if (le16(pkt + 6) == 6) on_smp(s, pkt + 8, n);
    else if (le16(pkt + 6) == 4) on_att(s, pkt + 8, n);
    return 1;
}

static int op_pump(void *ctx, int timeout_ms)
{
    sim_le_t *s = ctx;

    g_clock += timeout_ms > 0 ? timeout_ms : 1;
    if (s->powered && s->scanning && !s->connected && g_clock - s->t_adv >= 30) {
        /* flags, appearance gamepad, 16-bit services: HID */
        static const uint8_t ad[] = { 0x02, 0x01, 0x06, 0x03, 0x19, 0xC4, 0x03, 0x03, 0x03, 0x12, 0x18 };
        unsigned char r[48];
        r[0] = 0x02;
        r[1] = 1;
        r[2] = 0x00;                    /* ADV_IND */
        r[3] = s->cur_type;
        memcpy(r + 4, s->cur_addr, 6);
        r[10] = sizeof ad;
        memcpy(r + 11, ad, sizeof ad);
        r[11 + sizeof ad] = 0xC0;       /* RSSI */
        meta(s, r, 12 + (int)sizeof ad);
        s->t_adv = g_clock;
    }
    if (s->connected && s->encrypted && (s->cccd & 1) && g_clock - s->t_report >= 8 &&
        (!s->pad.wants_identity || s->identity == 2) && (!s->pad.needs_wake || s->awake)) {
        unsigned char f[80];
        f[0] = 0x1B;
        put16(f + 1, H_IN);
        memcpy(f + 3, s->pad.report, (size_t)s->pad.report_len);
        acl_to_host(s, 4, f, 3 + s->pad.report_len);
        s->t_report = g_clock;
    }
    return s->ev.count || s->acl.count;
}

static int op_next_event(void *ctx, unsigned char *out, int max)
{
    sim_le_t *s = ctx;
    int n;

    while ((n = q_get(&s->ev, out, max)) > 0)
        if (out[0] != 0x13 || !lose(s)) return n;
    return 0;
}

static int op_next_acl(void *ctx, unsigned char *out, int max)
{
    sim_le_t *s = ctx;
    int n;

    while ((n = q_get(&s->acl, out, max)) > 0)
        if (!lose(s)) return n;
    return 0;
}

static void op_close(void *ctx) { (void)ctx; }

static const hci_ops k_ops = { op_cmd, op_acl_send, op_pump, op_next_event, op_next_acl, op_close, NULL, NULL, NULL, NULL, NULL, NULL };

/* ---- public ------------------------------------------------------------------ */

sim_le_t *sim_le_new(const sim_le_pad *pad)
{
    sim_le_t *s = calloc(1, sizeof *s);

    s->pad = *pad;
    if (!s->pad.mtu) s->pad.mtu = 23;
    s->drop_state = pad->seed;
    memcpy(s->host_addr, k_host_addr, 6);
    s->powered = 1;
    srand(pad->seed + 1);
    new_address(s);
    ph_clock_hook = sim_le_now;
    return s;
}

void sim_le_free(sim_le_t *s) { free(s); }

hci_t sim_le_hci(sim_le_t *s)
{
    hci_t h = { &k_ops, s };
    return h;
}

void sim_le_inject_acl(sim_le_t *s, unsigned cid, const uint8_t *d, int n) { acl_to_host(s, cid, d, n); }
void sim_le_inject_event(sim_le_t *s, uint8_t code, const uint8_t *p, int n) { event(s, code, p, n); }

void sim_le_power_off(sim_le_t *s)
{
    disconnect(s, 0x13);
    s->powered = 0;
}

void sim_le_power_on(sim_le_t *s)
{
    s->powered = 1;
    new_address(s);
}

void sim_le_set_report(sim_le_t *s, const uint8_t *v, int n)
{
    memcpy(s->pad.report, v, (size_t)n);
    s->pad.report_len = n;
}

int sim_le_paired(const sim_le_t *s) { return s->paired; }
int sim_le_pairings(const sim_le_t *s) { return s->pairings; }
int sim_le_encrypted(const sim_le_t *s) { return s->encrypted; }
int sim_le_outputs(const sim_le_t *s) { return s->outputs; }
int sim_le_identity(const sim_le_t *s) { return s->identity == 2; }
int sim_le_creates(const sim_le_t *s) { return s->creates; }
int sim_le_cancels(const sim_le_t *s) { return s->cancels; }
int sim_le_blob_reads(const sim_le_t *s) { return s->blob_reads; }
const char *sim_le_error(const sim_le_t *s) { return s->error; }

int sim_le_last_output(const sim_le_t *s, uint8_t *out, int max)
{
    int n = s->last_out_len < max ? s->last_out_len : max;
    memcpy(out, s->last_out, (size_t)n);
    return n;
}
