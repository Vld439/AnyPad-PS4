#include "sim.h"
#include "../src/util.h"

#include <stdlib.h>
#include <string.h>

#define QLEN    256
#define PKT     1100
#define HANDLE  0x0041

/* The pad's ends of its channels. */
#define RCID_SDP  0x0070
#define RCID_CTRL 0x0071
#define RCID_INTR 0x0072

typedef struct {
    unsigned char d[QLEN][PKT];
    int len[QLEN];
    int head, count;
} queue;

typedef enum { RC_CLOSED, RC_CONNECTING, RC_CONFIG, RC_OPEN } rc_state;

typedef struct {
    unsigned psm, cid, peer;
    rc_state st;
    int cfg_in, cfg_out;
    long t_cfg;
} rchan;

struct sim {
    unsigned stale;                 /* handle+1 of a link left by an earlier run, 0: none */
    int channel_requests, cfg_responses;
    unsigned tx_hash;
    long trace_start;
    int stale_cleared;
    int disconnect_requests, inquiry_during_setup, create_requests;
    sim_pad pad;
    queue ev, acl;
    int connected, enc, has_key, got_output;
    unsigned char key[16];
    rchan sdp, ctrl, intr;
    unsigned char sig_id;
    int sdp_queries, sdp_conts, outputs, dropped, unplugs, n_inquiry, rejects;
    int feature_requests, got_feature;
    int pins, pin_ok;
    int wii_mode, wii_ext, wii_status_sent;
    long yield_until;               /* events go to "the system" until then */
    unsigned drop_count;
    unsigned char last_out[128];
    int last_out_len;
    long t_report;
    int deaf;
    int ssp, scan;
    int fail_pages_left;            /* the next calls get no answer (page timeout) */
    int drop_code, drop_left;       /* the next events with this code are lost */
    unsigned ps_iv, ps_win;         /* page scan activity of the "console" */
    int busy_radio;                 /* a heavy page scan leaves no time for searches and calls */
    unsigned char evmask[8];
    int ev_reads, left_to_system;   /* our event reads pending; events the "system" got */
    int incoming_pending;           /* the pad's own call to the console waits for an answer */
    int links_made;
    int empty_left, sent_complete, early_calls, n_calls;     /* cut-short answers still to send; a complete one sent; calls made before it */
};

static void trace_tx(sim_t *s, unsigned tag, const unsigned char *p, int n)
{
    int i;
    s->tx_hash = (s->tx_hash ^ tag) * 16777619u;
    s->tx_hash = (s->tx_hash ^ (unsigned)(sim_now() - s->trace_start)) * 16777619u;
    for (i = 0; i < n; i++) s->tx_hash = (s->tx_hash ^ p[i]) * 16777619u;
}

static long g_clock = 1000;

long sim_now(void) { return g_clock; }

/* ---- queues ----------------------------------------------------------------- */

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

/* ---- what the controller reports ------------------------------------------ */

static void event(sim_t *s, unsigned char code, const unsigned char *p, int n)
{
    unsigned char e[260];

    if (sim_now() < s->yield_until) return;     /* left to "the system" */
    if (s->ev_reads == 0) { s->left_to_system++; return; }   /* no reads of ours pending */
    if (s->drop_left > 0 && code == s->drop_code) { s->drop_left--; return; }

    e[0] = code;
    e[1] = (unsigned char)n;
    memcpy(e + 2, p, (size_t)n);
    q_put(&s->ev, e, 2 + n);
}

static void cmd_complete(sim_t *s, unsigned op, const unsigned char *ret, int n)
{
    unsigned char p[64];

    p[0] = 1;
    put16(p + 1, op);
    memcpy(p + 3, ret, (size_t)n);
    event(s, 0x0E, p, 3 + n);
}

static void cmd_ok(sim_t *s, unsigned op)
{
    static const unsigned char ok[1] = { 0 };
    cmd_complete(s, op, ok, 1);
}

static void cmd_status(sim_t *s, unsigned op, unsigned char status)
{
    unsigned char p[4];

    p[0] = status;
    p[1] = 1;
    put16(p + 2, op);
    event(s, 0x0F, p, 4);
}

static void ev_addr(sim_t *s, unsigned char code)
{
    event(s, code, s->pad.addr, 6);
}

static void ev_handle(sim_t *s, unsigned char code, unsigned char status, int extra, unsigned char x)
{
    unsigned char p[4];

    p[0] = status;
    put16(p + 1, HANDLE);
    p[3] = x;
    event(s, code, p, 3 + extra);
}

static void conn_complete(sim_t *s)
{
    s->links_made++;
    unsigned char p[11];

    p[0] = 0;
    put16(p + 1, HANDLE);
    memcpy(p + 3, s->pad.addr, 6);
    p[9] = 1;
    p[10] = 0;
    event(s, 0x03, p, 11);
    s->connected = 1;
}

static void new_key(sim_t *s)
{
    unsigned char p[23];
    int i;

    for (i = 0; i < 16; i++) s->key[i] = (unsigned char)(0xA0 + i);
    s->has_key = 1;
    memcpy(p, s->pad.addr, 6);
    memcpy(p + 6, s->key, 16);
    p[22] = 0x04;
    event(s, 0x18, p, 23);
}

/* ---- the pad's L2CAP ---------------------------------------------------- */

static void acl_to_host(sim_t *s, unsigned cid, const unsigned char *d, int n)
{
    unsigned char pkt[PKT];

    put16(pkt, HANDLE | 0x2000);
    put16(pkt + 2, (unsigned)(4 + n));
    put16(pkt + 4, (unsigned)n);
    put16(pkt + 6, cid);
    memcpy(pkt + 8, d, (size_t)n);
    q_put(&s->acl, pkt, 8 + n);
}

static void sig(sim_t *s, unsigned char code, unsigned char id, const unsigned char *d, int n)
{
    unsigned char p[64];

    p[0] = code;
    p[1] = id;
    put16(p + 2, (unsigned)n);
    memcpy(p + 4, d, (size_t)n);
    acl_to_host(s, 1, p, 4 + n);
}

static rchan *rc_by_cid(sim_t *s, unsigned cid)
{
    if (cid == RCID_SDP) return &s->sdp;
    if (cid == RCID_CTRL) return &s->ctrl;
    if (cid == RCID_INTR) return &s->intr;
    return NULL;
}

static rchan *rc_by_psm(sim_t *s, unsigned psm)
{
    if (psm == 0x01) return &s->sdp;
    if (psm == 0x11) return &s->ctrl;
    if (psm == 0x13) return &s->intr;
    return NULL;
}

static void rc_send_cfg(sim_t *s, rchan *c)
{
    unsigned char r[4];

    put16(r, c->peer);
    put16(r + 2, 0);
    sig(s, 0x04, ++s->sig_id, r, 4);
    c->t_cfg = g_clock;
}

static void rc_check(rchan *c)
{
    if (c->st == RC_CONFIG && c->cfg_in && c->cfg_out) c->st = RC_OPEN;
}

static void rc_connect(sim_t *s, rchan *c)
{
    unsigned char r[4];

    c->st = RC_CONNECTING;
    c->cfg_in = c->cfg_out = 0;
    put16(r, c->psm);
    put16(r + 2, c->cid);
    sig(s, 0x02, ++s->sig_id, r, 4);
}

static void on_host_signaling(sim_t *s, const unsigned char *d, int n)
{
    unsigned char code = d[0], id = d[1], r[8];
    const unsigned char *c = d + 4;
    rchan *ch;

    (void)n;
    switch (code) {
    case 0x02:  /* connection request: psm, scid */
        s->channel_requests++;
        if (s->pad.refuse_count < 0 || s->channel_requests <= s->pad.refuse_count ||
            (s->pad.refuse_first_link && s->links_made == 1)) {
            put16(r, 0); put16(r + 2, le16(c + 2));
            put16(r + 4, 4); put16(r + 6, 0);
            sig(s, 0x03, id, r, 8);
            break;
        }
        ch = rc_by_psm(s, le16(c));
        if (!ch) break;
        ch->peer = le16(c + 2);
        ch->st = RC_CONFIG;
        ch->cfg_in = ch->cfg_out = 0;
        put16(r, ch->cid);
        put16(r + 2, ch->peer);
        put16(r + 4, 0);
        put16(r + 6, 0);
        if (s->pad.early_cfg) rc_send_cfg(s, ch);
        sig(s, 0x03, id, r, 8);
        if (!s->pad.early_cfg) rc_send_cfg(s, ch);
        break;
    case 0x03:  /* connection response: dcid, scid, result */
        ch = rc_by_cid(s, le16(c + 2));
        if (!ch || le16(c + 4) != 0) break;
        ch->peer = le16(c);
        ch->st = RC_CONFIG;
        rc_send_cfg(s, ch);
        break;
    case 0x04:  /* configuration request: dcid (ours) */
        ch = rc_by_cid(s, le16(c));
        if (!ch) break;
        put16(r, ch->peer);
        put16(r + 2, 0);
        put16(r + 4, 0);
        sig(s, 0x05, id, r, 6);
        ch->cfg_in = 1;
        rc_check(ch);
        break;
    case 0x05:  /* configuration response: scid (ours) */
        s->cfg_responses++;
        ch = rc_by_cid(s, le16(c));
        if (!ch) break;
        ch->cfg_out = 1;
        rc_check(ch);
        break;
    case 0x06:  /* disconnection request: dcid (ours), scid */
        sig(s, 0x07, id, c, 4);
        ch = rc_by_cid(s, le16(c));
        if (ch) ch->st = RC_CLOSED;
        break;
    default:
        break;
    }
}

/* A data element sequence header around `len` bytes. */
static int seq(unsigned char *p, int len)
{
    if (len < 256) { p[0] = 0x35; p[1] = (unsigned char)len; return 2; }
    p[0] = 0x36; p[1] = (unsigned char)(len >> 8); p[2] = (unsigned char)len;
    return 3;
}

/* The attribute list the pad answers with, for the service asked. */
static int sdp_list(sim_t *s, unsigned uuid, unsigned char *out)
{
    unsigned char a[700];
    int n = 0, l = 0;

    if (uuid == 0x1200 && s->pad.no_device_id) {
        /* an empty attribute list */
    } else if (uuid == 0x1200) {       /* {0x0201 vid, 0x0202 pid, 0x0203 version} */
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x01;
        a[n++] = 0x09; a[n++] = (unsigned char)(s->pad.vid >> 8); a[n++] = (unsigned char)s->pad.vid;
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x02;
        a[n++] = 0x09; a[n++] = (unsigned char)(s->pad.pid >> 8); a[n++] = (unsigned char)s->pad.pid;
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x03;
        a[n++] = 0x09; a[n++] = 0x01; a[n++] = 0x00;
    } else if (uuid == 0x1124 && s->pad.desc_len) {
        /* {0x0206, {{0x22, descriptor}}} */
        unsigned char inner[600];
        int i = 0, dl = s->pad.desc_len;
        inner[i++] = 0x08; inner[i++] = 0x22;
        if (dl < 256) { inner[i++] = 0x25; inner[i++] = (unsigned char)dl; }
        else { inner[i++] = 0x26; inner[i++] = (unsigned char)(dl >> 8); inner[i++] = (unsigned char)dl; }
        memcpy(inner + i, s->pad.desc, (size_t)dl);
        i += dl;
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x06;
        {
            unsigned char mid[620];
            int m = seq(mid, i);
            memcpy(mid + m, inner, (size_t)i);
            m += i;
            n += seq(a + n, m);
            memcpy(a + n, mid, (size_t)m);
            n += m;
        }
    } else {
        return seq(out, 0);
    }
    /* one record's attribute list, inside the list of records */
    {
        unsigned char rec[720];
        int r = seq(rec, n);
        memcpy(rec + r, a, (size_t)n);
        r += n;
        l = seq(out, r);
        memcpy(out + l, rec, (size_t)r);
        l += r;
    }
    return l;
}

#define SDP_CHUNK 48

static void on_host_sdp(sim_t *s, const unsigned char *d, int n)
{
    unsigned char list[800], rsp[128];
    int len, p, cl, off = 0, chunk;
    unsigned uuid;

    if (n < 12) return;
    if (s->pad.sdp_two_step_only && d[0] == 0x06) {     /* not understood: Error Response */
        rsp[0] = 0x01; rsp[1] = d[1]; rsp[2] = d[2]; rsp[3] = 0; rsp[4] = 2; rsp[5] = 0; rsp[6] = 3;
        acl_to_host(s, s->sdp.peer, rsp, 7);
        return;
    }
    if (s->pad.sdp_two_step_only && d[0] == 0x02) {     /* Service Search: one handle per record */
        uuid = (unsigned)d[8] << 8 | d[9];
        len = sdp_list(s, uuid, list);
        rsp[0] = 0x03; rsp[1] = d[1]; rsp[2] = d[2];
        rsp[5] = 0; rsp[6] = len > 2; rsp[7] = 0; rsp[8] = len > 2;
        n = 9;
        if (len > 2) {
            rsp[9] = 0; rsp[10] = 1; rsp[11] = (unsigned char)(uuid >> 8); rsp[12] = (unsigned char)uuid;
            n = 13;
        }
        rsp[n++] = 0;                           /* no continuation */
        rsp[3] = 0;
        rsp[4] = (unsigned char)(n - 5);
        s->sdp_queries++;
        acl_to_host(s, s->sdp.peer, rsp, n);
        return;
    }
    if (s->pad.sdp_two_step_only && d[0] == 0x04 && n >= 14) {  /* Service Attribute */
        uuid = (unsigned)d[7] << 8 | d[8];      /* the handle carries the UUID */
        p = 11 + 2 + d[12];                     /* past handle, byte count, attribute list */
    } else {
        if (d[0] != 0x06 || s->pad.sdp_two_step_only) return;
        uuid = (unsigned)d[8] << 8 | d[9];      /* pattern {UUID16} */
        p = 5 + 2 + d[6];                       /* past the pattern */
        p += 2;                                 /* max byte count */
        p += 2 + d[p + 1];                      /* attribute id list */
    }
    if (p >= n) return;
    cl = d[p];
    if (cl == 2) {
        off = d[p + 1] << 8 | d[p + 2];
        s->sdp_conts++;
    } else {
        s->sdp_queries++;
    }

    len = sdp_list(s, uuid, list);
    chunk = len - off < SDP_CHUNK ? len - off : SDP_CHUNK;
    rsp[0] = d[0] == 0x04 ? 0x05 : 0x07;
    rsp[1] = d[1];
    rsp[2] = d[2];
    rsp[5] = (unsigned char)(chunk >> 8);
    rsp[6] = (unsigned char)chunk;
    memcpy(rsp + 7, list + off, (size_t)chunk);
    if (off + chunk < len) {
        rsp[7 + chunk] = 2;
        rsp[8 + chunk] = (unsigned char)((off + chunk) >> 8);
        rsp[9 + chunk] = (unsigned char)(off + chunk);
        n = 10 + chunk;
    } else {
        rsp[7 + chunk] = 0;
        n = 8 + chunk;
    }
    rsp[3] = (unsigned char)((n - 5) >> 8);
    rsp[4] = (unsigned char)(n - 5);
    acl_to_host(s, s->sdp.peer, rsp, n);
}

/* ---- hci_ops ------------------------------------------------------------- */

static int op_cmd(void *ctx, unsigned op, const void *params, int plen)
{
    sim_t *s = ctx;
    const unsigned char *p = params;

    (void)plen;
    if (s->deaf) return 1;
    trace_tx(s, op, p, plen);
    switch (op) {
    case 0x0C55: {      /* Read Simple Pairing Mode */
        unsigned char r[2] = { 0, (unsigned char)s->ssp };
        cmd_complete(s, op, r, 2);
        break;
    }
    case 0x0C56:
        s->ssp = p[0];
        cmd_ok(s, op);
        break;
    case 0x0C19: {      /* Read Scan Enable */
        unsigned char r[2] = { 0, (unsigned char)s->scan };
        cmd_complete(s, op, r, 2);
        break;
    }
    case 0x0C1A:
        s->scan = p[0];
        cmd_ok(s, op);
        break;
    case 0x0C1B: {      /* Read Page Scan Activity */
        unsigned char r[5] = { 0 };
        put16(r + 1, s->ps_iv);
        put16(r + 3, s->ps_win);
        cmd_complete(s, op, r, 5);
        break;
    }
    case 0x0C1C:
        s->ps_iv = le16(p);
        s->ps_win = le16(p + 2);
        cmd_ok(s, op);
        break;
    case 0x0C01:        /* Set Event Mask */
        memcpy(s->evmask, p, 8);
        cmd_ok(s, op);
        break;
    case 0x1005: {      /* Read Buffer Size */
        unsigned char r[8] = { 0 };
        put16(r + 1, 1021);
        r[3] = 64;
        put16(r + 4, 8);
        cmd_complete(s, op, r, 8);
        break;
    }
    case 0x0401:        /* Inquiry */
        s->n_inquiry++;
        if (s->connected && !s->enc) s->inquiry_during_setup++;
        cmd_status(s, op, 0);
        if (s->busy_radio && s->ps_win * 8 > s->ps_iv) break;     /* nothing heard */
        if (s->pad.mode == SIM_WAIT_PAIRING && !s->connected) {
            unsigned char r[255] = { 0 };
            r[0] = 1;
            memcpy(r + 1, s->pad.addr, 6);
            r[7] = 1;
            r[9] = (unsigned char)s->pad.cod;
            r[10] = (unsigned char)(s->pad.cod >> 8);
            r[11] = (unsigned char)(s->pad.cod >> 16);
            put16(r + 12, 0x1234);
            if (s->empty_left > 0) {
                s->empty_left--;                /* cut short: no data at all */
            } else {
                static const unsigned char name[6] = { 0x05, 0x09, 'P', 'a', 'd', '1' };
                if (s->pad.eir_len > 0) memcpy(r + 15, s->pad.eir, (size_t)s->pad.eir_len);
                else memcpy(r + 15, name, sizeof name);     /* real pads announce a name */
                s->sent_complete = 1;
            }
            event(s, 0x2F, r, 255);
        }
        break;
    case 0x0405:        /* Create Connection */
        s->create_requests++;
        if (s->stale) {                 /* an earlier run's link still holds the pad */
            cmd_status(s, op, 0x0B);    /* connection already exists */
            break;
        }
        if (s->incoming_pending && memcmp(p, s->pad.addr, 6) == 0) {
            cmd_status(s, op, 0x0B);    /* connection already exists: its call is pending */
            break;
        }
        cmd_status(s, op, 0);
        if (memcmp(p, s->pad.addr, 6) == 0) s->n_calls++;
        if (memcmp(p, s->pad.addr, 6) == 0 && s->pad.eir_empty_rounds > 0 && !s->sent_complete) s->early_calls++;
        if (memcmp(p, s->pad.addr, 6) == 0 &&
            (s->fail_pages_left > 0 || (s->busy_radio && s->ps_win * 8 > s->ps_iv))) {
            unsigned char f[11];
            if (s->fail_pages_left > 0) s->fail_pages_left--;
            memset(f, 0, sizeof f);
            f[0] = 0x04;                /* page timeout */
            memcpy(f + 3, s->pad.addr, 6);
            f[9] = 1;
            event(s, 0x03, f, 11);
            break;
        }
        if (memcmp(p, s->pad.addr, 6) == 0) conn_complete(s);
        break;
    case 0x040A:        /* Reject Connection Request */
        s->rejects++;
        if (s->incoming_pending && memcmp(p, s->pad.addr, 6) == 0) {
            unsigned char f[11];
            s->incoming_pending = 0;
            cmd_status(s, op, 0);
            memset(f, 0, sizeof f);
            f[0] = p[6];                /* the reason given */
            memcpy(f + 3, s->pad.addr, 6);
            f[9] = 1;
            event(s, 0x03, f, 11);
        } else if (s->stale) {
            cmd_status(s, op, 0x02);    /* no such call: a real link */
        } else {
            cmd_status(s, op, 0);
        }
        break;
    case 0x0409:        /* Accept Connection Request */
        cmd_status(s, op, 0);
        conn_complete(s);
        /* A pad reconnecting by itself authenticates itself. */
        ev_addr(s, 0x17);
        break;
    case 0x0411:        /* Authentication Requested */
        cmd_status(s, op, 0);
        ev_addr(s, 0x17);
        break;
    case 0x040C:        /* Link Key Negative Reply: pair */
        cmd_ok(s, op);
        ev_addr(s, s->pad.legacy_pin ? 0x16 : 0x31);   /* PIN Code Request, or IO Capability Request */
        break;
    case 0x040D: {      /* PIN Code Reply: address, length, PIN */
        static const unsigned char host[6] = { 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 };
        s->pins++;
        cmd_ok(s, op);
        if (s->pad.legacy_pin == 1) s->pin_ok = p[6] == 6 && memcmp(p + 7, host, 6) == 0;
        else s->pin_ok = p[6] == 4 && memcmp(p + 7, "0000", 4) == 0;
        if (s->pin_ok) {
            new_key(s);
            ev_handle(s, 0x06, 0, 0, 0);
        } else {
            ev_handle(s, 0x06, 0x05, 0, 0);     /* authentication failure */
        }
        break;
    }
    case 0x1009: {      /* Read BD_ADDR: this controller's own address */
        static const unsigned char host[6] = { 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 };
        unsigned char r[7];
        r[0] = 0;
        memcpy(r + 1, host, 6);
        cmd_complete(s, op, r, 7);
        break;
    }
    case 0x042B: {      /* IO Capability Reply */
        unsigned char r[10] = { 0 };
        cmd_ok(s, op);
        memcpy(r, s->pad.addr, 6);
        event(s, 0x33, r, 10);
        break;
    }
    case 0x042C:        /* User Confirmation Reply */
        cmd_ok(s, op);
        new_key(s);
        ev_handle(s, 0x06, 0, 0, 0);
        break;
    case 0x040B:        /* Link Key Reply */
        cmd_ok(s, op);
        if (s->has_key && memcmp(p + 6, s->key, 16) == 0) {
            if (s->pad.mode == SIM_RECONNECT) {
                s->enc = 1;                     /* the pad encrypts by itself */
                ev_handle(s, 0x08, 0, 1, 1);
            } else {
                ev_handle(s, 0x06, 0, 0, 0);
            }
        } else {
            ev_handle(s, 0x06, 0x05, 0, 0);     /* authentication failure */
        }
        break;
    case 0x0413:        /* Set Connection Encryption */
        cmd_status(s, op, 0);
        s->enc = 1;
        ev_handle(s, 0x08, 0, 1, 1);
        break;
    case 0x0406:        /* Disconnect */
        s->disconnect_requests++;
        if (s->stale) {
            if (le16(p) == s->stale - 1) {
                s->stale = 0;
                s->stale_cleared++;
                cmd_status(s, op, 0);
            } else {
                cmd_status(s, op, 0x02);    /* unknown connection handle */
            }
            break;
        }
        cmd_status(s, op, 0);
        sim_disconnect(s);
        break;
    default:
        cmd_ok(s, op);
        break;
    }
    return 1;
}

static int op_acl_send(void *ctx, const unsigned char *pkt, int len)
{
    sim_t *s = ctx;
    unsigned cid;
    const unsigned char *d;
    int n;
    unsigned char ncp[5];

    trace_tx(s, 0x10000, pkt, len);
    if (len < 8 || !s->connected) return 1;
    n = (int)le16(pkt + 4);
    cid = le16(pkt + 6);
    d = pkt + 8;

    ncp[0] = 1;
    put16(ncp + 1, HANDLE);
    put16(ncp + 3, 1);
    event(s, 0x13, ncp, 5);

    if (cid == 1) on_host_signaling(s, d, n);
    else if (cid == RCID_SDP && s->sdp.st == RC_OPEN) on_host_sdp(s, d, n);
    else if (cid == RCID_CTRL && n >= 1 && d[0] == 0x15) s->unplugs++;   /* HID virtual cable unplug */
    else if (cid == RCID_CTRL && n >= 2 && d[0] == 0x43) {               /* GET_REPORT, feature */
        unsigned char r[42];
        s->feature_requests++;
        memset(r, 0, sizeof r);
        r[0] = 0xA3;                    /* DATA, feature */
        r[1] = d[1];
        acl_to_host(s, s->ctrl.peer, r, sizeof r);
        if (s->pad.wake_feature && d[1] == s->pad.wake_feature) s->got_feature = 1;
    }
    else if (cid == RCID_INTR && n >= 2 && d[0] == 0xA2 && s->pad.wii) {
        s->outputs++;
        if (d[1] == 0x12 && n >= 4) s->wii_mode = d[3];
        if (d[1] == 0x16 && n >= 8) {               /* write: flags, address, size, data */
            unsigned addr = (unsigned)d[3] << 16 | (unsigned)d[4] << 8 | d[5];
            unsigned char ack[6] = { 0xA1, 0x22, s->pad.basic[0], s->pad.basic[1], 0x16, 0x00 };
            if (addr == 0xA400F0 && d[7] == 0x55) s->wii_ext = 1;
            if (addr == 0xA400FB && d[7] == 0x00 && s->wii_ext >= 1) s->wii_ext = 2;
            acl_to_host(s, s->intr.peer, ack, sizeof ack);
        }
    }
    else if (cid == RCID_INTR && n >= 2 && d[0] == 0xA2) {
        s->outputs++;
        if (s->outputs > s->pad.ignore_outputs) s->got_output = 1;
        s->last_out_len = n < (int)sizeof s->last_out ? n : (int)sizeof s->last_out;
        memcpy(s->last_out, d, (size_t)s->last_out_len);
    }
    return 1;
}

/* The pad's own behaviour between packets. */
static void pad_step(sim_t *s)
{
    rchan *all[3];
    int i;

    if (!s->connected || !s->enc) return;

    /* L2CAP's retransmission timer: an unanswered request is sent again. */
    all[0] = &s->sdp; all[1] = &s->ctrl; all[2] = &s->intr;
    for (i = 0; i < 3; i++)
        if (!s->pad.no_cfg_repeat && all[i]->st == RC_CONFIG && !all[i]->cfg_out && g_clock - all[i]->t_cfg >= 1000)
            rc_send_cfg(s, all[i]);

    if (s->pad.mode == SIM_RECONNECT) {
        if (s->ctrl.st == RC_CLOSED) rc_connect(s, &s->ctrl);
        else if (s->ctrl.st == RC_OPEN && s->intr.st == RC_CLOSED) rc_connect(s, &s->intr);
    }
    if (s->pad.wii && s->intr.st == RC_OPEN) {
        unsigned char f[24];
        if (!s->wii_status_sent) {              /* on connecting: its status, then quiet */
            s->wii_status_sent = 1;
            sim_wii_status(s);
        } else if (s->wii_mode && g_clock - s->t_report >= 8) {
            f[0] = 0xA1;
            f[1] = (unsigned char)s->wii_mode;
            f[2] = s->pad.basic[0];
            f[3] = s->pad.basic[1];
            if (s->wii_mode == 0x34) {
                if (s->pad.wii == 1 && s->wii_ext == 2) memcpy(f + 4, s->pad.report, 19);
                else memset(f + 4, 0, 19);
                acl_to_host(s, s->intr.peer, f, 23);
            } else if (s->wii_mode == 0x30) {
                acl_to_host(s, s->intr.peer, f, 4);
            }
            s->t_report = g_clock;
        }
        return;
    }
    if (s->intr.st == RC_OPEN && g_clock - s->t_report >= 8) {
        unsigned char f[130];
        int full = s->pad.wake_feature ? s->got_feature : s->got_output || !s->pad.needs_output;
        const unsigned char *r = full ? s->pad.report : s->pad.basic;
        int n = full ? s->pad.report_len : s->pad.basic_len;
        if (n > 0) {
            f[0] = 0xA1;
            memcpy(f + 1, r, (size_t)n);
            acl_to_host(s, s->intr.peer, f, 1 + n);
        }
        s->t_report = g_clock;
    }
}

static int op_pump(void *ctx, int timeout_ms)
{
    sim_t *s = ctx;

    g_clock += timeout_ms > 0 ? timeout_ms : 1;
    if (s->deaf) return 0;
    pad_step(s);
    return s->ev.count || s->acl.count;
}

/* Drops every Nth ACL packet and completion report on the way to the host,
 * as sharing the controller with the system's driver does. */
static int lose(sim_t *s)
{
    /* Pseudo-random with a fixed seed: a fixed period could keep hitting
     * the same packet of a retry that sends exactly that many. */
    s->drop_count = s->drop_count * 1103515245u + 12345u;
    if (!s->pad.drop_every || (s->drop_count >> 16) % (unsigned)s->pad.drop_every) return 0;
    s->dropped++;
    return 1;
}

static int op_next_event(void *ctx, unsigned char *out, int max)
{
    sim_t *s = ctx;
    int n;

    while ((n = q_get(&s->ev, out, max)) > 0)
        if (out[0] != 0x13 || !lose(s)) return n;
    return 0;
}

static int op_next_acl(void *ctx, unsigned char *out, int max)
{
    sim_t *s = ctx;
    int n;

    while ((n = q_get(&s->acl, out, max)) > 0)
        if (!lose(s)) return n;
    return 0;
}

static void op_close(void *ctx) { (void)ctx; }

static int g_diag_calls;
static void op_diag(void *ctx) { (void)ctx; g_diag_calls++; }
int sim_diag_calls(void) { return g_diag_calls; }
static int g_yields;
static void op_yield(void *ctx, int ms) { g_yields++; ((sim_t *)ctx)->yield_until = sim_now() + ms; }
int sim_yields(void) { return g_yields; }

static void op_event_reads(void *ctx, int n) { ((sim_t *)ctx)->ev_reads = n; }

static const hci_ops k_sim_ops = {
    op_cmd, op_acl_send, op_pump, op_next_event, op_next_acl, op_close, op_diag, op_yield, NULL, op_event_reads, NULL, NULL,
};

/* ---- public ---------------------------------------------------------------- */

sim_t *sim_new(const sim_pad *pad)
{
    sim_t *s = calloc(1, sizeof *s);

    s->pad = *pad;
    s->trace_start = sim_now();
    s->empty_left = pad->eir_empty_rounds;
    s->ps_iv = 0x0800;
    s->ev_reads = 40;
    s->ps_win = 0x0012;
    s->drop_count = pad->seed;
    s->sdp = (rchan){ 0x01, RCID_SDP, 0, RC_CLOSED, 0, 0, 0 };
    s->ctrl = (rchan){ 0x11, RCID_CTRL, 0, RC_CLOSED, 0, 0, 0 };
    s->intr = (rchan){ 0x13, RCID_INTR, 0, RC_CLOSED, 0, 0, 0 };
    ph_clock_hook = sim_now;
    return s;
}

void sim_free(sim_t *s) { free(s); }

hci_t sim_hci(sim_t *s)
{
    hci_t h = { &k_sim_ops, s };
    return h;
}

void sim_power_on(sim_t *s)
{
    unsigned char p[10];

    memcpy(p, s->pad.addr, 6);
    p[6] = (unsigned char)s->pad.cod;
    p[7] = (unsigned char)(s->pad.cod >> 8);
    p[8] = (unsigned char)(s->pad.cod >> 16);
    p[9] = 1;
    event(s, 0x04, p, 10);
}

void sim_disconnect(sim_t *s)
{
    unsigned char p[4];

    if (!s->connected) return;
    p[0] = 0;
    put16(p + 1, HANDLE);
    p[3] = 0x13;
    event(s, 0x05, p, 4);
    s->connected = s->enc = s->got_output = s->got_feature = 0;
    s->sdp.st = s->ctrl.st = s->intr.st = RC_CLOSED;
    s->wii_mode = s->wii_ext = s->wii_status_sent = 0;
}

/* The pad drops the link and the host never hears of it (the event went
 * to the system, or was lost); later it calls again by itself. */
void sim_vanish(sim_t *s)
{
    s->connected = s->enc = s->got_output = s->got_feature = 0;
    s->sdp.st = s->ctrl.st = s->intr.st = RC_CLOSED;
    s->pad.mode = SIM_RECONNECT;
}

void sim_set_report(sim_t *s, const unsigned char *r, int len)
{
    memcpy(s->pad.report, r, (size_t)len);
    s->pad.report_len = len;
}

void sim_inject_acl(sim_t *s, unsigned cid, const unsigned char *d, int n) { acl_to_host(s, cid, d, n); }
void sim_inject_event(sim_t *s, unsigned char code, const unsigned char *p, int n) { event(s, code, p, n); }

void sim_go_deaf(sim_t *s)
{
    s->deaf = 1;
    s->ev.count = s->acl.count = 0;
}

void sim_give_key(sim_t *s, const unsigned char *key)
{
    memcpy(s->key, key, 16);
    s->has_key = 1;
}

int sim_sdp_queries(const sim_t *s) { return s->sdp_queries; }
int sim_outputs(const sim_t *s) { return s->outputs; }
int sim_feature_requests(const sim_t *s) { return s->feature_requests; }
int sim_pins(const sim_t *s) { return s->pins; }
int sim_pin_ok(const sim_t *s) { return s->pin_ok; }

void sim_wii_status(sim_t *s)
{
    unsigned char f[8] = { 0xA1, 0x20, s->pad.basic[0], s->pad.basic[1],
                           (unsigned char)(s->pad.wii == 1 ? 0x02 : 0x00), 0, 0, 0xC0 };
    s->wii_mode = 0;
    acl_to_host(s, s->intr.peer, f, sizeof f);
}
int sim_unplugs(const sim_t *s) { return s->unplugs; }
int sim_rejects(const sim_t *s) { return s->rejects; }
int sim_inquiries(const sim_t *s) { return s->n_inquiry; }
int sim_sdp_continuations(const sim_t *s) { return s->sdp_conts; }
int sim_link_up(const sim_t *s) { return s->connected; }
int sim_dropped(const sim_t *s) { return s->dropped; }
int sim_scan(const sim_t *s) { return s->scan; }
void sim_stale_link(sim_t *s, unsigned handle) { s->stale = handle + 1; }
int sim_stale_cleared(const sim_t *s) { return s->stale_cleared; }
int sim_early_calls(const sim_t *s) { return s->early_calls; }
int sim_calls(const sim_t *s) { return s->n_calls; }
void sim_fail_pages(sim_t *s, int n) { s->fail_pages_left = n; }
void sim_set_page_scan(sim_t *s, unsigned iv, unsigned win) { s->ps_iv = iv; s->ps_win = win; }
void sim_get_page_scan(const sim_t *s, unsigned *iv, unsigned *win) { *iv = s->ps_iv; *win = s->ps_win; }
void sim_busy_radio(sim_t *s, int on) { s->busy_radio = on; }
const unsigned char *sim_event_mask(const sim_t *s) { return s->evmask; }
int sim_event_reads(const sim_t *s) { return s->ev_reads; }
void sim_pad_calls_pending(sim_t *s) { s->incoming_pending = 1; }
int sim_left_to_system(const sim_t *s) { return s->left_to_system; }
void sim_drop_events(sim_t *s, int code, int n) { s->drop_code = code; s->drop_left = n; }
void sim_set_scan(sim_t *s, int v) { s->scan = v; }

int sim_last_output(const sim_t *s, unsigned char *out, int max)
{
    int n = s->last_out_len < max ? s->last_out_len : max;
    memcpy(out, s->last_out, (size_t)n);
    return n;
}

int sim_disconnect_requests(const sim_t *s) { return s->disconnect_requests; }
int sim_inquiry_during_setup(const sim_t *s) { return s->inquiry_during_setup; }

int sim_create_requests(const sim_t *s) { return s->create_requests; }

int sim_channel_requests(const sim_t *s) { return s->channel_requests; }
unsigned sim_tx_hash(const sim_t *s) { return s->tx_hash; }
int sim_cfg_responses(const sim_t *s) { return s->cfg_responses; }
