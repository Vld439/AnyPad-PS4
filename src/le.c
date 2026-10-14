/* Bluetooth LE game controllers: HID over GATT (HOGP), as current Xbox
 * controllers and many others speak it.
 *
 *   scanning    pads advertise; in the pairing window any that looks like a
 *               gamepad is taken, otherwise only paired ones (also behind
 *               resolvable private addresses, through their IRK)
 *   pairing     SMP as initiator, "Just Works": LE Secure Connections when
 *               both sides can (the controller does the P-256 maths), LE
 *               legacy otherwise. The keys the pad hands out are stored.
 *   encryption  with the long-term key, on every reconnection
 *   GATT        PnP ID (vendor, product), the HID service, its reports and
 *               their Report References, the Report Map when no profile
 *               knows the pad, then notifications on for every input report
 *
 * Input reports arrive as notifications and go through the same profiles
 * as Classic pads; output reports go out as Write Commands.
 */
#include "host_int.h"
#include "log.h"
#include "smp_crypto.h"
#include "util.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define OP_LE_SET_EVENT_MASK     0x2001
#define OP_LE_READ_BUFFER_SIZE   0x2002
#define OP_LE_SET_SCAN_PARAMS    0x200B
#define OP_LE_SET_SCAN_ENABLE    0x200C
#define OP_LE_CREATE_CONN        0x200D
#define OP_LE_CREATE_CONN_CANCEL 0x200E
#define OP_LE_CONN_UPDATE        0x2013
#define OP_LE_START_ENCRYPTION   0x2019
#define OP_LE_READ_LOCAL_P256    0x2025
#define OP_LE_GENERATE_DHKEY     0x2026

#define CID_ATT     0x0004
#define CID_LE_SIG  0x0005
#define CID_SMP     0x0006

/* SMP commands. */
#define SMP_PAIRING_REQ     0x01
#define SMP_PAIRING_RSP     0x02
#define SMP_CONFIRM         0x03
#define SMP_RANDOM          0x04
#define SMP_PAIRING_FAILED  0x05
#define SMP_ENC_INFO        0x06
#define SMP_CENTRAL_ID      0x07
#define SMP_ID_INFO         0x08
#define SMP_ID_ADDR         0x09
#define SMP_SECURITY_REQ    0x0B
#define SMP_PUBLIC_KEY      0x0C
#define SMP_DHKEY_CHECK     0x0D

/* Pairing as initiator. */
enum {
    SMP_IDLE, SMP_WAIT_RSP, SMP_WAIT_PKEY, SMP_WAIT_CONFIRM, SMP_WAIT_RANDOM,
    SMP_WAIT_DHKEY, SMP_WAIT_CHECK, SMP_ENCRYPTING, SMP_KEYS, SMP_DONE, SMP_FAILED,
};

/* GATT discovery. */
enum {
    G_IDLE, G_MTU, G_PNP, G_SVC, G_CHARS, G_DESCS, G_REFS, G_MAP, G_CCCD, G_DONE, G_FAILED,
};

#define UUID_HID_SERVICE    0x1812
#define UUID_PRIMARY        0x2800
#define UUID_CHAR_DECL      0x2803
#define UUID_CCCD           0x2902
#define UUID_REPORT_REF     0x2908
#define UUID_REPORT_MAP     0x2A4B
#define UUID_REPORT         0x2A4D
#define UUID_PNP_ID         0x2A50
#define UUID_CONTROL_POINT  0x2A4C  /* HID Control Point: 1 = Exit Suspend */

#define OUR_MTU             185

#define T_LE_CONNECT     8000   /* LE Create Connection to Connection Complete */
#define LE_BUSY_TRIES       3   /* asks again after a busy refusal (0x0c) ... */
#define T_LE_REST        5000   /* ... then the pad is left alone this long */
#define T_SMP_STEP       4000   /* no pairing progress: start over (SMP never repeats) */
#define T_ENC            4000   /* encryption with a stored key */
#define T_KEYS           3000   /* the pad's keys after encryption */
#define T_ATT_RESEND     1500
#define ATT_TRIES           4
#define T_SCAN_REFRESH   5000
#define T_SETUP         30000

/* ---- helpers --------------------------------------------------------------- */

/* 16 random bytes per call: AES in counter mode, keyed from the system's
 * entropy when there is any and from the clock regardless. */
static void rand_bytes(uint8_t *out, int n)
{
    static uint8_t key[16];
    static uint32_t counter;
    static int seeded;

    if (!seeded) {
        int fd = open("/dev/urandom", O_RDONLY);
        long t = now_ms();
        if (fd >= 0) {
            if (read(fd, key, sizeof key) != (ssize_t)sizeof key) memset(key, 0x5A, sizeof key);
            close(fd);
        }
        key[0] ^= (uint8_t)t;
        key[1] ^= (uint8_t)(t >> 8);
        key[2] ^= (uint8_t)(t >> 16);
        seeded = 1;
    }
    while (n > 0) {
        uint8_t in[16] = { 0 }, block[16];
        long t = now_ms();
        int k = n < 16 ? n : 16;

        counter++;
        put32(in, counter);
        put32(in + 4, (uint32_t)t);
        aes128(key, in, block);
        memcpy(out, block, (size_t)k);
        out += k;
        n -= k;
    }
}

static const uint8_t *link_id_addr(const link_t *l)
{
    return l->le_s.id_addr[0] | l->le_s.id_addr[1] | l->le_s.id_addr[2] |
           l->le_s.id_addr[3] | l->le_s.id_addr[4] | l->le_s.id_addr[5]
           ? l->le_s.id_addr : l->addr;
}

static db_rec *link_db(host_t *h, const link_t *l)
{
    return db_find(h, link_id_addr(l));
}

/* A paired LE pad behind this advertised address: by address, or a
 * resolvable private address its IRK resolves. */
static db_rec *known_pad(host_t *h, uint8_t type, const uint8_t *a)
{
    int i;

    for (i = 0; i < h->ndb; i++) {
        db_rec *d = &h->db[i];
        if (d->le && d->addr_type == type && memcmp(d->addr, a, 6) == 0) return d;
    }
    if (type != 1 || (a[5] & 0xC0) != 0x40) return NULL;   /* not resolvable */
    for (i = 0; i < h->ndb; i++) {
        db_rec *d = &h->db[i];
        uint8_t irk[16];
        uint32_t hash = (uint32_t)a[0] | (uint32_t)a[1] << 8 | (uint32_t)a[2] << 16;
        uint32_t prand = (uint32_t)a[3] | (uint32_t)a[4] << 8 | (uint32_t)a[5] << 16;
        if (!d->le || !d->has_irk) continue;
        smp_reverse(irk, d->irk, 16);
        if (smp_ah(irk, prand) == hash) return d;
    }
    return NULL;
}

/* ---- setup -------------------------------------------------------------------- */

void le_setup(host_t *h)
{
    /* LE events: connection complete, advertising report, connection
     * update, remote features, LTK request, ..., P-256 key, DHKey,
     * enhanced connection complete. */
    static const uint8_t mask[8] = { 0xFF, 0x03, 0, 0, 0, 0, 0, 0 };
    /* Active scanning (names come in scan responses), 60 ms every 60 ms. */
    static const uint8_t scan[7] = { 0x01, 0x60, 0x00, 0x60, 0x00, 0x00, 0x00 };

    if (!hci_sync(h, OP_LE_SET_EVENT_MASK, mask, 8)) {
        log_line("le: the controller does not do LE");
        h->le_ok = 0;
        return;
    }
    h->le_ok = 1;
    if (hci_sync(h, OP_LE_READ_BUFFER_SIZE, NULL, 0) && h->cc_len >= 9 &&
        h->cc[8] && le16(h->cc + 6)) {
        h->le_separate = 1;
        h->le_mtu = le16(h->cc + 6);
        h->lep.credits = h->lep.max = h->cc[8];
        log_line("le: %d LE buffers of %u bytes", h->lep.max, h->le_mtu);
    }
    hci_sync(h, OP_LE_SET_SCAN_PARAMS, scan, 7);
    /* Answered by an LE event; without it, pairing falls back to legacy. */
    send_cmd(h, OP_LE_READ_LOCAL_P256, NULL, 0);
}

/* Stops scanning if we started it. */
void le_shutdown(host_t *h)
{
    static const uint8_t off[2] = { 0, 0 };

    int i;

    if (h->le_ok && h->le_scanning) {
        hci_sync(h, OP_LE_SET_SCAN_ENABLE, off, 2);
        h->le_scanning = 0;
    }
    /* An attempt of ours left pending would refuse the next run's (0x0c). */
    for (i = 0; i < HOST_MAX_PADS; i++)
        if (h->links[i].used && h->links[i].le && !h->links[i].connected &&
            (h->links[i].le_s.create_out || h->links[i].le_s.retry_at)) {
            hci_sync(h, OP_LE_CREATE_CONN_CANCEL, NULL, 0);
            break;
        }
}

/* ---- sending -------------------------------------------------------------------- */

static void smp_send(host_t *h, link_t *l, uint8_t code, const uint8_t *d, int n)
{
    uint8_t p[72];

    p[0] = code;
    memcpy(p + 1, d, (size_t)n);
    l2_send(h, l, CID_SMP, p, n + 1);
    l->le_s.t_smp = now_ms();
}

/* Sends `n` bytes as the same bytes reversed: values held MSB first go
 * over the air LSB first. */
static void smp_send_rev(host_t *h, link_t *l, uint8_t code, const uint8_t *msb, int n)
{
    uint8_t v[64];

    smp_reverse(v, msb, (size_t)n);
    smp_send(h, l, code, v, n);
}

static void att_send(host_t *h, link_t *l, const uint8_t *req, int n)
{
    le_link *e = &l->le_s;

    if (req != e->req) {
        memcpy(e->req, req, (size_t)n);
        e->req_len = n;
        e->req_tries = 0;
    }
    e->req_tries++;
    e->t_req = now_ms();
    l2_send(h, l, CID_ATT, e->req, e->req_len);
}

static void start_encryption(host_t *h, link_t *l, const uint8_t ltk_lsb[16],
                             const uint8_t rand[8], uint16_t ediv)
{
    uint8_t p[28];

    put16(p, l->handle);
    memcpy(p + 2, rand, 8);
    put16(p + 10, ediv);
    memcpy(p + 12, ltk_lsb, 16);
    send_cmd(h, OP_LE_START_ENCRYPTION, p, 28);
    l->le_s.enc_sent = 1;
    l->le_s.t_enc = now_ms();
}

/* ---- connecting -------------------------------------------------------------- */

static void le_create(host_t *h, link_t *l);

static void le_connect(host_t *h, uint8_t type, const uint8_t *addr, db_rec *bond)
{
    link_t *l = link_new(h, addr);

    if (!l) return;
    l->le = 1;
    l->le_s.addr_type = type;
    if (bond) {
        l->le_s.bonded = 1;
        memcpy(l->le_s.id_addr, bond->addr, 6);
        l->le_s.id_type = bond->addr_type;
        if (bond->vid || bond->pid) {
            l->vid = bond->vid;
            l->pid = bond->pid;
            l->ids_known = 1;
        }
    } else {
        l->pairing = 1;
    }
    le_create(h, l);
    log_line("le pad %s: connecting%s", addr_str(addr), bond ? "" : " to pair");
}

/* Asks the controller to connect to the pad. */
static void le_create(host_t *h, link_t *l)
{
    uint8_t p[25], off[2] = { 0, 0 };

    /* Some controllers will not scan and connect at once. */
    send_cmd(h, OP_LE_SET_SCAN_ENABLE, off, 2);
    h->le_scanning = 0;

    put16(p, 0x0060);                   /* scan interval */
    put16(p + 2, 0x0030);               /* scan window */
    p[4] = 0;                           /* this address, not the accept list */
    p[5] = l->le_s.addr_type;
    memcpy(p + 6, l->addr, 6);
    p[12] = 0;                          /* our public address */
    put16(p + 13, 6);                   /* interval 7.5 .. 15 ms */
    put16(p + 15, 12);
    put16(p + 17, 0);                   /* no latency */
    put16(p + 19, 200);                 /* supervision timeout 2 s */
    put16(p + 21, 0);
    put16(p + 23, 0);
    send_cmd(h, OP_LE_CREATE_CONN, p, 25);
    l->le_s.create_out = 1;
    l->le_s.retry_at = 0;
    l->t_create = now_ms();
}

/* The controller's answer to our LE Create Connection. A busy refusal
 * (0x0c) means another attempt is still pending in it: someone else's, or
 * one an earlier run left. Freeing the link and asking again at the next
 * advert, as before, only repeats the refusal. The pending attempt is
 * cancelled and ours is made again a little later; after a few refusals
 * the pad is left alone for a while. Answers to commands that are not ours
 * (the system's) are left alone. */
void le_on_create_status(host_t *h, int status)
{
    static const long wait[LE_BUSY_TRIES] = { 300, 800, 1500 };
    link_t *l = NULL;
    le_link *e;
    int i;

    for (i = 0; i < HOST_MAX_PADS; i++)
        if (h->links[i].used && h->links[i].le && !h->links[i].connected && h->links[i].le_s.create_out)
            l = &h->links[i];
    if (!l) {
        if (status) log_line("le: someone else's connection attempt was refused (status %#04x)", status);
        return;
    }
    e = &l->le_s;
    e->create_out = 0;
    if (status == 0) return;
    if (status == 0x0C && e->refused < LE_BUSY_TRIES) {
        send_cmd(h, OP_LE_CREATE_CONN_CANCEL, NULL, 0);
        e->retry_at = now_ms() + wait[e->refused];
        e->refused++;
        log_line("le pad %s: the controller was busy with another connection attempt: "
                 "cancelled it, trying again in %ld ms", addr_str(l->addr), wait[e->refused - 1]);
        return;
    }
    if (status == 0x0C) {
        memcpy(h->le_rest_addr, l->addr, 6);
        h->le_rest_until = now_ms() + T_LE_REST;
        log_line("le pad %s: still refused; left alone for %d s", addr_str(l->addr), T_LE_REST / 1000);
    }
    log_line("pad %s: connection refused (status %#04x)", addr_str(l->addr), status);
    link_free(h, l);
}

/* Does this advertising data look like a game controller: the gamepad or
 * joystick appearance, or the HID service with a controller-like name. */
static int looks_like_gamepad(const uint8_t *d, int n)
{
    int i = 0, hid = 0, appearance = 0, named = 0;

    while (i + 1 < n) {
        int len = d[i], type;
        const uint8_t *v = d + i + 2;
        if (len == 0 || i + 1 + len > n) break;
        type = d[i + 1];
        if (type == 0x19 && len >= 3) {
            unsigned a = le16(v);
            appearance = a == 0x03C4 || a == 0x03C3;
        } else if ((type == 0x02 || type == 0x03) && len >= 3) {
            int k;
            for (k = 0; k + 1 < len - 1; k += 2) if (le16(v + k) == UUID_HID_SERVICE) hid = 1;
        } else if (type == 0x08 || type == 0x09) {
            char name[32];
            int k, m = len - 1 < 31 ? len - 1 : 31;
            for (k = 0; k < m; k++) name[k] = (char)(v[k] | 0x20);   /* lower case */
            name[m] = '\0';
            named = strstr(name, "controller") || strstr(name, "gamepad") || strstr(name, "pad");
        }
        i += 1 + len;
    }
    return appearance || (hid && named);
}

static void on_advert(host_t *h, const uint8_t *r, int n)
{
    uint8_t evtype, type, dlen;
    const uint8_t *addr, *data;
    db_rec *bond;
    int i;

    if (n < 10) return;
    evtype = r[0];
    type = r[1];
    addr = r + 2;
    dlen = r[8];
    data = r + 9;
    if (9 + dlen > n || evtype > 1) return;     /* connectable adverts only */
    if (link_by_addr(h, addr)) return;
    if (now_ms() < h->le_rest_until && memcmp(addr, h->le_rest_addr, 6) == 0) return;
    for (i = 0; i < HOST_MAX_PADS; i++)         /* one connection attempt at a time */
        if (h->links[i].used && h->links[i].le && !h->links[i].connected) return;

    bond = known_pad(h, type, addr);
    if (bond) {
        for (i = 0; i < HOST_MAX_PADS; i++)
            if (h->links[i].used && h->links[i].le &&
                memcmp(link_id_addr(&h->links[i]), bond->addr, 6) == 0) return;
        le_connect(h, type, addr, bond);
    } else if (now_ms() < h->pair_until && looks_like_gamepad(data, dlen)) {
        log_line("found LE gamepad %s", addr_str(addr));
        le_connect(h, type, addr, NULL);
    }
}

void le_poll(host_t *h, long now)
{
    int want = 0, i, j;
    uint8_t p[2];

    if (!h->le_ok) return;
    if (now < h->pair_until) want = 1;
    for (i = 0; i < h->ndb && !want; i++) {
        int linked = 0;
        if (!h->db[i].le) continue;
        for (j = 0; j < HOST_MAX_PADS; j++)
            if (h->links[j].used && h->links[j].le &&
                memcmp(link_id_addr(&h->links[j]), h->db[i].addr, 6) == 0) linked = 1;
        if (!linked) want = 1;
    }
    for (i = 0; i < HOST_MAX_PADS; i++)
        if (h->links[i].used && h->links[i].le && !h->links[i].connected) want = 0;
    /* Nor while a Classic pad is being called or set up: the scan takes the
     * radio all the time, and the pad, in pairing mode for a short while,
     * would not hear the call (page timeout). */
    for (i = 0; i < HOST_MAX_PADS; i++)
        if (h->links[i].used && !h->links[i].le && !h->links[i].ready) want = 0;

    /* Sent again now and then: its reply may have gone astray. */
    if (want != h->le_scanning || (want && now - h->t_scan > T_SCAN_REFRESH)) {
        p[0] = (uint8_t)want;
        p[1] = 0;
        send_cmd(h, OP_LE_SET_SCAN_ENABLE, p, 2);
        h->le_scanning = want;
        h->t_scan = now;
    }
}

/* ---- HCI LE events ----------------------------------------------------------- */

void le_on_meta(host_t *h, const unsigned char *ev, int len)
{
    link_t *l;

    switch (ev[0]) {
    case 0x01:  /* Connection Complete */
    case 0x0A:  /* Enhanced Connection Complete */
        if (len < 12) break;
        l = link_by_addr(h, ev + 6);
        if (!l || !l->le || l->connected) break;
        if (ev[1] == 0x02 && l->le_s.retry_at) break;  /* the attempt we cancelled ended */
        if (ev[1] != 0) {
            log_line("le pad %s: connection failed (status %#04x)", addr_str(l->addr), ev[1]);
            link_free(h, l);
            break;
        }
        l->le_s.create_out = 0;
        l->le_s.retry_at = 0;
        l->connected = 1;
        l->handle = le16(ev + 2) & 0x0FFF;
        l->t_conn = now_ms();
        log_line("le pad %s: connected, handle %#05x", addr_str(l->addr), l->handle);
        break;

    case 0x02:  /* Advertising Report: one report per event, in practice */
        if (len >= 3 && ev[1] >= 1) on_advert(h, ev + 2, len - 2);
        break;

    case 0x08:  /* Read Local P-256 Public Key Complete */
        if (len >= 66 && ev[1] == 0) {
            memcpy(h->le_pk, ev + 2, 64);
            h->le_pk_ok = 1;
        } else {
            log_line("le: no P-256 key from the controller, legacy pairing only");
        }
        break;

    case 0x09:  /* Generate DHKey Complete */
        l = h->dhkey_owner;
        h->dhkey_owner = NULL;
        if (!l || !l->used) break;
        if (len >= 34 && ev[1] == 0) {
            memcpy(l->le_s.dhkey, ev + 2, 32);
            l->le_s.have_dhkey = 1;
        } else {
            log_line("le pad %s: DHKey failed (status %#04x)", addr_str(l->addr), ev[1]);
            l->le_s.smp = SMP_FAILED;
            link_disconnect(h, l, "pairing failed");
        }
        break;

    default:
        break;
    }
}

/* ---- pairing -------------------------------------------------------------- */

static void smp_fail(host_t *h, link_t *l, uint8_t reason, const char *why)
{
    smp_send(h, l, SMP_PAIRING_FAILED, &reason, 1);
    l->le_s.smp = SMP_FAILED;
    log_line("le pad %s: pairing failed: %s", addr_str(l->addr), why);
    link_disconnect(h, l, "pairing failed");
}

/* Initiator and responder as the 7-byte "type || address" SC functions use. */
static void sc_addrs(host_t *h, link_t *l, uint8_t a[7], uint8_t b[7])
{
    a[0] = 0;                           /* our public address */
    smp_reverse(a + 1, h->bdaddr, 6);
    b[0] = l->le_s.addr_type;
    smp_reverse(b + 1, l->addr, 6);
}

static void legacy_confirm(host_t *h, link_t *l, const uint8_t r[16], uint8_t out[16])
{
    static const uint8_t tk[16];        /* Just Works */
    uint8_t preq[7], pres[7], ia[6], ra[6];

    smp_reverse(preq, l->le_s.preq, 7);
    smp_reverse(pres, l->le_s.pres, 7);
    smp_reverse(ia, h->bdaddr, 6);
    smp_reverse(ra, l->addr, 6);
    smp_c1(tk, r, preq, pres, 0, l->le_s.addr_type, ia, ra, out);
}

/* Our identity key, made once and kept beside the pairing store. */
static void load_host_irk(host_t *h)
{
    char path[sizeof h->db_path + 8];
    FILE *f;

    if (h->le_irk_ok) return;
    snprintf(path, sizeof path, "%s.irk", h->db_path);
    f = fopen(path, "rb");
    h->le_irk_ok = f && fread(h->le_irk, 1, 16, f) == 16;
    if (f) fclose(f);
    if (h->le_irk_ok) return;
    rand_bytes(h->le_irk, 16);
    f = fopen(path, "wb");
    if (f) {
        (void)fwrite(h->le_irk, 1, 16, f);
        fclose(f);
    }
    h->le_irk_ok = 1;
}

/* After the pad's keys, a pad that agreed to it gets ours: its identity key
 * and our public address. Some pads (the Xbox Series) take the bond as
 * unfinished without them: they keep blinking and send no input. */
static void send_host_identity(host_t *h, link_t *l)
{
    le_link *e = &l->le_s;
    uint8_t identity[7];

    if (e->identity_sent || !(e->pres[5] & e->preq[5] & 0x02)) return;
    load_host_irk(h);
    smp_send(h, l, SMP_ID_INFO, h->le_irk, 16);
    identity[0] = 0;                    /* public address, in HCI order */
    memcpy(identity + 1, h->bdaddr, 6);
    smp_send(h, l, SMP_ID_ADDR, identity, sizeof identity);
    e->identity_sent = 1;
    log_line("le pad %s: our identity handed over", addr_str(l->addr));
}

static void bond_save(host_t *h, link_t *l)
{
    le_link *e = &l->le_s;
    db_rec *d;

    send_host_identity(h, l);

    if (e->keys_got & 0x02) {           /* the pad's identity address */
        d = db_put(h, e->id_addr);
        d->addr_type = e->id_type;
        memcpy(d->irk, e->dist_irk, 16);    /* kept as sent: LSB first */
        d->has_irk = 1;
    } else {
        memcpy(e->id_addr, l->addr, 6);
        e->id_type = e->addr_type;
        d = db_put(h, l->addr);
        d->addr_type = e->addr_type;
        d->has_irk = 0;
    }
    d->le = 1;
    if (e->sc) {
        smp_reverse(d->key, e->ltk, 16);
        d->ediv = 0;
        memset(d->rand, 0, 8);
    } else {
        memcpy(d->key, e->dist_ltk, 16);
        d->ediv = e->dist_ediv;
        memcpy(d->rand, e->dist_rand, 8);
    }
    if (l->ids_known) {
        d->vid = l->vid;
        d->pid = l->pid;
    }
    db_save(h);
    e->bonded = 1;
    e->smp = SMP_DONE;
    log_line("le pad %s: paired (%s), keys stored", addr_str(l->addr), e->sc ? "Secure Connections" : "legacy");
}

static void smp_start(host_t *h, link_t *l)
{
    le_link *e = &l->le_s;

    e->preq[0] = SMP_PAIRING_REQ;
    e->preq[1] = 0x03;                  /* NoInputNoOutput */
    e->preq[2] = 0x00;                  /* no OOB data */
    e->preq[3] = (uint8_t)(0x01 | (h->le_pk_ok ? 0x08 : 0));   /* bonding, SC */
    e->preq[4] = 16;                    /* key size */
    e->preq[5] = 0x02;                  /* our identity, if the pad takes it */
    e->preq[6] = 0x03;                  /* the pad's LTK and identity */
    smp_send(h, l, SMP_PAIRING_REQ, e->preq + 1, 6);
    e->smp = SMP_WAIT_RSP;
}

static void smp_step(host_t *h, link_t *l, long now)
{
    le_link *e = &l->le_s;

    if (e->smp == SMP_IDLE) {
        smp_start(h, l);
        return;
    }
    if (e->smp == SMP_DONE || e->smp == SMP_FAILED || e->smp == SMP_KEYS) return;

    if (e->sc && e->have_pkb && !e->dhkey_asked && !h->dhkey_owner) {
        h->dhkey_owner = l;
        send_cmd(h, OP_LE_GENERATE_DHKEY, e->pkb, 64);
        e->dhkey_asked = 1;
    }
    if (e->smp == SMP_WAIT_DHKEY && e->have_dhkey) {
        /* Both nonces in, DHKey known: derive the keys and prove it. */
        uint8_t w[32], a[7], b[7], mackey[16], ea[16], io[3], r[16] = { 0 };
        smp_reverse(w, e->dhkey, 32);
        sc_addrs(h, l, a, b);
        smp_f5(w, e->na, e->nb, a, b, mackey, e->ltk);
        io[0] = e->preq[3];
        io[1] = e->preq[2];
        io[2] = e->preq[1];
        smp_f6(mackey, e->na, e->nb, r, io, a, b, ea);
        memcpy(e->cb, mackey, 16);      /* keep MacKey for the pad's check */
        smp_send_rev(h, l, SMP_DHKEY_CHECK, ea, 16);
        e->smp = SMP_WAIT_CHECK;
    }
    if (now - e->t_smp > T_SMP_STEP) smp_fail(h, l, 0x08, "timed out");
}

static void on_smp(host_t *h, link_t *l, const uint8_t *d, int n)
{
    le_link *e = &l->le_s;
    uint8_t v[16];

    if (n < 1) return;
    switch (d[0]) {
    case SMP_PAIRING_RSP:
        if (e->smp != SMP_WAIT_RSP || n < 7) break;
        memcpy(e->pres, d, 7);
        e->sc = (e->preq[3] & 0x08) && (d[3] & 0x08);
        e->keys_expected = d[6] & (e->sc ? 0x02 : 0x03);
        if (e->sc) {
            smp_send(h, l, SMP_PUBLIC_KEY, h->le_pk, 64);
            e->smp = SMP_WAIT_PKEY;
        } else {
            uint8_t c[16];
            rand_bytes(e->na, 16);
            legacy_confirm(h, l, e->na, c);
            smp_send_rev(h, l, SMP_CONFIRM, c, 16);
            e->smp = SMP_WAIT_CONFIRM;
        }
        break;

    case SMP_PUBLIC_KEY:
        if (e->smp != SMP_WAIT_PKEY || n < 65) break;
        memcpy(e->pkb, d + 1, 64);
        e->have_pkb = 1;
        rand_bytes(e->na, 16);
        e->smp = SMP_WAIT_CONFIRM;      /* the pad commits first */
        e->t_smp = now_ms();
        break;

    case SMP_CONFIRM:
        if (e->smp != SMP_WAIT_CONFIRM || n < 17) break;
        smp_reverse(e->cb, d + 1, 16);
        e->have_cb = 1;
        smp_send_rev(h, l, SMP_RANDOM, e->na, 16);
        e->smp = SMP_WAIT_RANDOM;
        break;

    case SMP_RANDOM:
        if (e->smp != SMP_WAIT_RANDOM || n < 17) break;
        smp_reverse(e->nb, d + 1, 16);
        if (e->sc) {
            uint8_t pkbx[32], pkax[32];
            smp_reverse(pkbx, e->pkb, 32);
            smp_reverse(pkax, h->le_pk, 32);
            smp_f4(pkbx, pkax, e->nb, 0, v);
            if (memcmp(v, e->cb, 16) != 0) {
                smp_fail(h, l, 0x04, "the pad's confirm value does not match");
                break;
            }
            e->smp = SMP_WAIT_DHKEY;
            e->t_smp = now_ms();
            smp_step(h, l, now_ms());
        } else {
            static const uint8_t tk[16];
            uint8_t stk[16], lsb[16];
            static const uint8_t zero[8];
            legacy_confirm(h, l, e->nb, v);
            if (memcmp(v, e->cb, 16) != 0) {
                smp_fail(h, l, 0x04, "the pad's confirm value does not match");
                break;
            }
            smp_s1(tk, e->nb, e->na, stk);
            memcpy(e->ltk, stk, 16);
            smp_reverse(lsb, stk, 16);
            start_encryption(h, l, lsb, zero, 0);
            e->smp = SMP_ENCRYPTING;
        }
        break;

    case SMP_DHKEY_CHECK:
        if (e->smp != SMP_WAIT_CHECK || n < 17) break;
        {
            uint8_t a[7], b[7], io[3], r[16] = { 0 }, eb[16], lsb[16];
            static const uint8_t zero[8];
            sc_addrs(h, l, a, b);
            io[0] = e->pres[3];
            io[1] = e->pres[2];
            io[2] = e->pres[1];
            smp_f6(e->cb, e->nb, e->na, r, io, b, a, v);    /* e->cb holds MacKey */
            smp_reverse(eb, d + 1, 16);
            if (memcmp(v, eb, 16) != 0) {
                smp_fail(h, l, 0x0B, "the pad's DHKey check does not match");
                break;
            }
            smp_reverse(lsb, e->ltk, 16);
            start_encryption(h, l, lsb, zero, 0);
            e->smp = SMP_ENCRYPTING;
        }
        break;

    case SMP_PAIRING_FAILED:
        log_line("le pad %s: the pad refused pairing (reason %#04x)", addr_str(l->addr),
                 n > 1 ? d[1] : 0);
        e->smp = SMP_FAILED;
        link_disconnect(h, l, "pairing refused");
        break;

    case SMP_ENC_INFO:
        if (n >= 17) memcpy(e->dist_ltk, d + 1, 16);
        break;
    case SMP_CENTRAL_ID:
        if (n < 11) break;
        e->dist_ediv = (uint16_t)le16(d + 1);
        memcpy(e->dist_rand, d + 3, 8);
        e->keys_got |= 0x01;
        break;
    case SMP_ID_INFO:
        if (n >= 17) memcpy(e->dist_irk, d + 1, 16);
        break;
    case SMP_ID_ADDR:
        if (n < 8) break;
        e->id_type = d[1];
        memcpy(e->id_addr, d + 2, 6);
        e->keys_got |= 0x02;
        break;

    case SMP_SECURITY_REQ:              /* the pad asks for security */
        if (!l->enc_on && e->bonded && !e->enc_sent) {
            db_rec *r = link_db(h, l);
            if (r) start_encryption(h, l, r->key, r->rand, r->ediv);
        }
        break;

    default:
        break;
    }

    if (e->smp == SMP_KEYS && (e->keys_got & e->keys_expected) == e->keys_expected)
        bond_save(h, l);
}

void le_on_encryption(host_t *h, link_t *l, int status, int on)
{
    le_link *e = &l->le_s;

    if (status == 0 && on) {
        if (!l->enc_on) log_line("le pad %s: link encrypted", addr_str(l->addr));
        l->enc_on = 1;
        l->auth_ok = 1;
        if (e->smp == SMP_ENCRYPTING) {
            e->smp = SMP_KEYS;
            e->t_smp = now_ms();
            /* The keys may have come in the same batch as this event. */
            if ((e->keys_got & e->keys_expected) == e->keys_expected) bond_save(h, l);
        }
        return;
    }
    log_line("le pad %s: encryption failed (status %#04x)", addr_str(l->addr), status);
    if (e->bonded && !l->pairing && (status == 0x06 || status == 0x05)) {
        log_line("le pad %s: it forgot us; pair it again", addr_str(l->addr));
        db_forget(h, link_id_addr(l));
    }
    link_disconnect(h, l, "encryption failed");
}

/* ---- GATT ---------------------------------------------------------------------- */

static int is_input_report(const gatt_char *c)
{
    return c->uuid == UUID_REPORT && (c->rtype == 1 || (!c->ref && (c->props & 0x10)));
}

/* The next characteristic from index `from` that `want` accepts, or -1. */
static int next_char(le_link *e, int from, int (*want)(const gatt_char *))
{
    int i;

    for (i = from; i < e->nch; i++) if (want(&e->ch[i])) return i;
    return -1;
}

static int wants_descs(const gatt_char *c) { return c->uuid == UUID_REPORT && c->value < c->end; }
static int wants_ref(const gatt_char *c) { return c->uuid == UUID_REPORT && c->ref; }
static int wants_cccd(const gatt_char *c) { return is_input_report(c) && c->cccd; }

static void read_by_type(host_t *h, link_t *l, uint16_t start, uint16_t end, uint16_t uuid)
{
    uint8_t q[7];

    q[0] = 0x08;
    put16(q + 1, start);
    put16(q + 3, end);
    put16(q + 5, uuid);
    att_send(h, l, q, 7);
}

static void read_handle(host_t *h, link_t *l, uint16_t handle)
{
    uint8_t q[3];

    q[0] = 0x0A;
    put16(q + 1, handle);
    att_send(h, l, q, 3);
}

/* Moves to `state`, sending its first request. */
static void wake_hid(host_t *h, link_t *l);

static void gatt_go(host_t *h, link_t *l, int state)
{
    le_link *e = &l->le_s;
    uint8_t q[8];
    int i;

    e->gatt = state;
    switch (state) {
    case G_MTU:
        q[0] = 0x02;
        put16(q + 1, OUR_MTU);
        att_send(h, l, q, 3);
        break;
    case G_PNP:
        read_by_type(h, l, 0x0001, 0xFFFF, UUID_PNP_ID);
        break;
    case G_SVC:
        {   /* Find By Type Value: primary services whose value is HID */
            uint8_t r[9];
            r[0] = 0x06;
            put16(r + 1, 0x0001);
            put16(r + 3, 0xFFFF);
            put16(r + 5, UUID_PRIMARY);
            put16(r + 7, UUID_HID_SERVICE);
            att_send(h, l, r, 9);
        }
        break;
    case G_CHARS:
        read_by_type(h, l, e->cursor, e->svc_end, UUID_CHAR_DECL);
        break;
    case G_DESCS:
        i = next_char(e, e->idx, wants_descs);
        if (i < 0) {
            e->idx = 0;
            gatt_go(h, l, G_REFS);
            break;
        }
        e->idx = i;
        q[0] = 0x04;
        put16(q + 1, (uint16_t)(e->ch[i].value + 1));
        put16(q + 3, e->ch[i].end);
        att_send(h, l, q, 5);
        break;
    case G_REFS:
        i = next_char(e, e->idx, wants_ref);
        if (i < 0) {
            e->idx = 0;
            if (l->prof) {
                gatt_go(h, l, G_CCCD);
            } else {
                e->map_len = 0;
                gatt_go(h, l, G_MAP);
            }
            break;
        }
        e->idx = i;
        read_handle(h, l, e->ch[i].ref);
        break;
    case G_MAP:
        if (!e->map_handle) {
            link_disconnect(h, l, "no profile and no report map");
            e->gatt = G_FAILED;
            break;
        }
        if (e->map_len == 0) {
            read_handle(h, l, e->map_handle);
        } else {
            q[0] = 0x0C;                /* read blob */
            put16(q + 1, e->map_handle);
            put16(q + 3, (uint16_t)e->map_len);
            att_send(h, l, q, 5);
        }
        break;
    case G_CCCD:
        i = next_char(e, e->idx, wants_cccd);
        if (i < 0) {
            wake_hid(h, l);
            e->gatt = G_DONE;
            break;
        }
        e->idx = i;
        q[0] = 0x12;
        put16(q + 1, e->ch[i].cccd);
        q[3] = 0x01;                    /* notifications on */
        q[4] = 0x00;
        att_send(h, l, q, 5);
        break;
    default:
        break;
    }
}

/* HID over GATT: a pad that offers a Control Point is told to leave suspend
 * (a write without response; nothing is waited for). Pads without one are
 * left alone. */
static void wake_hid(host_t *h, link_t *l)
{
    le_link *e = &l->le_s;
    uint8_t q[4];
    int i;

    if (e->suspend_off) return;
    for (i = 0; i < e->nch; i++) {
        if (e->ch[i].uuid != UUID_CONTROL_POINT || !(e->ch[i].props & 0x04)) continue;
        q[0] = 0x52;                    /* write command */
        put16(q + 1, e->ch[i].value);
        q[3] = 0x01;                    /* Exit Suspend */
        l2_send(h, l, CID_ATT, q, 4);
        e->suspend_off = 1;
        log_line("le pad %s: told to leave suspend", addr_str(l->addr));
        return;
    }
}

/* Settles which profile reads this pad, once its ids are known. */
static void choose_profile(link_t *l)
{
    l->prof = profile_find(l->vid, l->pid);
    if (l->prof && l->prof->setup) l->prof->setup(l->pctx.bytes, l->vid, l->pid);
    if (!l->prof)
        log_line("le pad %s: %04x:%04x has no profile, reading its report map",
                 addr_str(l->addr), l->vid, l->pid);
}

static void chars_done(host_t *h, link_t *l)
{
    le_link *e = &l->le_s;
    int i;

    for (i = 0; i < e->nch; i++) {
        e->ch[i].end = (uint16_t)(i + 1 < e->nch ? e->ch[i + 1].decl - 1 : e->svc_end);
        if (e->ch[i].uuid == UUID_REPORT_MAP) e->map_handle = e->ch[i].value;
    }
    choose_profile(l);
    e->idx = 0;
    gatt_go(h, l, G_DESCS);
}

/* A response to our pending request. */
static void on_att_response(host_t *h, link_t *l, const uint8_t *d, int n)
{
    le_link *e = &l->le_s;
    int err = d[0] == 0x01 && n >= 5 && d[1] == e->req[0];
    int i;

    if (!err && d[0] != e->req[0] + 1) return;      /* not the answer we wait for */
    e->req_len = 0;                                 /* answered */

    switch (e->gatt) {
    case G_MTU:
        if (!err && n >= 3) {
            unsigned m = le16(d + 1);
            e->mtu = (uint16_t)(m < OUR_MTU ? (m < 23 ? 23 : m) : OUR_MTU);
        }
        gatt_go(h, l, l->ids_known ? G_SVC : G_PNP);
        break;

    case G_PNP:
        /* Read By Type response: entry length, then handle + PnP ID:
         * vendor id source, vendor, product, version. */
        if (!err && n >= 2 + 2 + 7 && d[1] >= 9) {
            db_rec *r;
            l->vid = (uint16_t)le16(d + 5);
            l->pid = (uint16_t)le16(d + 7);
            log_line("le pad %s: vendor %04x product %04x", addr_str(l->addr), l->vid, l->pid);
            if ((r = link_db(h, l)) != NULL) {
                r->vid = l->vid;
                r->pid = l->pid;
                db_save(h);
            }
        }
        l->ids_known = 1;
        gatt_go(h, l, G_SVC);
        break;

    case G_SVC:
        if (err || n < 5) {
            link_disconnect(h, l, "no HID service");
            e->gatt = G_FAILED;
            break;
        }
        e->svc_start = (uint16_t)le16(d + 1);
        e->svc_end = (uint16_t)le16(d + 3);
        e->cursor = e->svc_start;
        e->nch = 0;
        gatt_go(h, l, G_CHARS);
        break;

    case G_CHARS:
        if (err || n < 2) {
            chars_done(h, l);
            break;
        }
        {
            int len = d[1];
            uint16_t last = e->cursor;
            if (len < 7) {
                chars_done(h, l);
                break;
            }
            for (i = 2; i + len <= n; i += len) {
                gatt_char *c;
                last = (uint16_t)le16(d + i);
                if (e->nch == LE_MAX_CHARS) break;
                c = &e->ch[e->nch++];
                memset(c, 0, sizeof *c);
                c->decl = last;
                c->props = d[i + 2];
                c->value = (uint16_t)le16(d + i + 3);
                c->uuid = len == 7 ? (uint16_t)le16(d + i + 5) : 0;
            }
            if (last >= e->svc_end || e->nch == LE_MAX_CHARS) {
                chars_done(h, l);
            } else {
                e->cursor = (uint16_t)(last + 1);
                gatt_go(h, l, G_CHARS);
            }
        }
        break;

    case G_DESCS:
        if (!err && n >= 2 && d[1] == 1) {          /* handle + 16-bit uuid pairs */
            gatt_char *c = &e->ch[e->idx];
            for (i = 2; i + 4 <= n; i += 4) {
                uint16_t hd = (uint16_t)le16(d + i), u = (uint16_t)le16(d + i + 2);
                if (u == UUID_REPORT_REF) c->ref = hd;
                else if (u == UUID_CCCD) c->cccd = hd;
            }
        }
        e->idx++;
        gatt_go(h, l, G_DESCS);
        break;

    case G_REFS:
        if (!err && n >= 3) {
            e->ch[e->idx].rid = d[1];
            e->ch[e->idx].rtype = d[2];
        }
        e->idx++;
        gatt_go(h, l, G_REFS);
        break;

    case G_MAP:
        if (err) {
            link_disconnect(h, l, "report map unreadable");
            e->gatt = G_FAILED;
            break;
        }
        {
            int got = n - 1;
            if (e->map_len + got > (int)sizeof e->map) got = (int)sizeof e->map - e->map_len;
            memcpy(e->map + e->map_len, d + 1, (size_t)got);
            e->map_len += got;
            if (n - 1 >= e->mtu - 1 && e->map_len < (int)sizeof e->map) {
                gatt_go(h, l, G_MAP);   /* there is more */
                break;
            }
        }
        if (!generic_setup(l->pctx.bytes, e->map, e->map_len, l->vid, l->pid, h->map_dir)) {
            link_disconnect(h, l, "no usable report map");
            e->gatt = G_FAILED;
            break;
        }
        l->prof = &generic_profile;
        e->idx = 0;
        gatt_go(h, l, G_CCCD);
        break;

    case G_CCCD:
        e->idx++;
        gatt_go(h, l, G_CCCD);
        break;

    default:
        break;
    }
}

/* Requests from the pad's own GATT client: we offer no attributes. */
static void on_att_request(host_t *h, link_t *l, const uint8_t *d, int n)
{
    uint8_t r[5];

    if (d[0] == 0x02 && n >= 3) {               /* Exchange MTU */
        r[0] = 0x03;
        put16(r + 1, OUR_MTU);
        l2_send(h, l, CID_ATT, r, 3);
        return;
    }
    r[0] = 0x01;
    r[1] = d[0];
    put16(r + 2, n >= 3 ? le16(d + 1) : 0);
    r[4] = (d[0] == 0x04 || d[0] == 0x06 || d[0] == 0x08 || d[0] == 0x10)
           ? 0x0A                               /* attribute not found */
           : 0x06;                              /* request not supported */
    l2_send(h, l, CID_ATT, r, 5);
}

static void on_att(host_t *h, link_t *l, const uint8_t *d, int n)
{
    le_link *e = &l->le_s;
    uint8_t op;

    if (n < 1) return;
    op = d[0];
    if (op == 0x1B || op == 0x1D) {             /* notification, indication */
        uint16_t hd;
        int i;
        if (op == 0x1D) {
            uint8_t c = 0x1E;
            l2_send(h, l, CID_ATT, &c, 1);
        }
        if (n < 3) return;
        hd = (uint16_t)le16(d + 1);
        for (i = 0; i < e->nch; i++) {
            const gatt_char *c = &e->ch[i];
            uint8_t rep[256];
            if (c->value != hd || !is_input_report(c)) continue;
            if (n - 3 > (int)sizeof rep - 1) return;
            if (c->rid) {
                rep[0] = c->rid;
                memcpy(rep + 1, d + 3, (size_t)(n - 3));
                link_input(h, l, rep, n - 2);
            } else {
                link_input(h, l, d + 3, n - 3);
            }
            return;
        }
        return;
    }
    if (op == 0x52 || op == 0xD2) return;       /* commands need no answer */
    if (op & 1 || op == 0x1E) {                 /* a response or confirmation */
        if (e->req_len) on_att_response(h, l, d, n);
        return;
    }
    on_att_request(h, l, d, n);
}

static void on_le_signaling(host_t *h, link_t *l, const uint8_t *d, int n)
{
    uint8_t r[6];

    if (n < 4) return;
    if (d[0] == 0x12 && n >= 12) {              /* connection parameter update request */
        uint8_t p[14];
        r[0] = 0x13;
        r[1] = d[1];
        put16(r + 2, 2);
        put16(r + 4, 0);                        /* accepted */
        l2_send(h, l, CID_LE_SIG, r, 6);
        put16(p, l->handle);
        memcpy(p + 2, d + 4, 8);                /* interval min, max, latency, timeout */
        put16(p + 10, 0);
        put16(p + 12, 0);
        send_cmd(h, OP_LE_CONN_UPDATE, p, 14);
    } else if (!(d[0] & 1) && d[0] != 0x12) {   /* other requests: rejected */
        r[0] = 0x01;
        r[1] = d[1];
        put16(r + 2, 2);
        put16(r + 4, 0);                        /* command not understood */
        l2_send(h, l, CID_LE_SIG, r, 6);
    }
}

void le_on_frame(host_t *h, link_t *l, unsigned cid, const unsigned char *d, int len)
{
    if (cid == CID_ATT) on_att(h, l, d, len);
    else if (cid == CID_SMP) on_smp(h, l, d, len);
    else if (cid == CID_LE_SIG) on_le_signaling(h, l, d, len);
}

int le_send_report(host_t *h, link_t *l, const unsigned char *rep, int len)
{
    le_link *e = &l->le_s;
    uint8_t q[80];
    int i;

    if (len < 1 || len - 1 > (int)sizeof q - 3 || len - 1 > e->mtu - 3) return 0;
    for (i = 0; i < e->nch; i++) {
        const gatt_char *c = &e->ch[i];
        if (c->uuid != UUID_REPORT || c->rtype != 2 || c->rid != rep[0]) continue;
        q[0] = 0x52;                            /* write command */
        put16(q + 1, c->value);
        memcpy(q + 3, rep + 1, (size_t)(len - 1));
        return l2_send(h, l, CID_ATT, q, len + 2);
    }
    return 0;
}

/* ---- the link's state machine ----------------------------------------------- */

void le_step(host_t *h, link_t *l, long now)
{
    le_link *e = &l->le_s;

    if (!l->connected) {
        if (e->retry_at) {
            if (now >= e->retry_at) le_create(h, l);
            return;
        }
        if (now - l->t_create > T_LE_CONNECT) {
            send_cmd(h, OP_LE_CREATE_CONN_CANCEL, NULL, 0);
            log_line("le pad %s: did not connect", addr_str(l->addr));
            link_free(h, l);
        }
        return;
    }
    if (l->disconnecting) return;
    if (!l->ready && now - l->t_conn > T_SETUP) {
        link_disconnect(h, l, "setup did not finish");
        return;
    }

    if (!l->enc_on) {
        if (e->bonded) {
            db_rec *r = link_db(h, l);
            if (!r) {
                link_disconnect(h, l, "no stored key");
            } else if (!e->enc_sent) {
                start_encryption(h, l, r->key, r->rand, r->ediv);
            } else if (now - e->t_enc > T_ENC) {
                link_disconnect(h, l, "encryption not answered");
            }
        } else {
            smp_step(h, l, now);
        }
        return;
    }
    if (e->smp == SMP_KEYS && now - e->t_smp > T_KEYS) {
        log_line("le pad %s: not every key came, storing what did", addr_str(l->addr));
        bond_save(h, l);
    }

    /* GATT: one request at a time, repeated if its answer went astray. */
    if (e->gatt == G_IDLE) {
        e->mtu = 23;
        gatt_go(h, l, G_MTU);
    } else if (e->gatt != G_DONE && e->gatt != G_FAILED && e->req_len &&
               now - e->t_req > T_ATT_RESEND) {
        if (e->req_tries >= ATT_TRIES) {
            e->gatt = G_FAILED;
            link_disconnect(h, l, "GATT not answered");
            return;
        }
        att_send(h, l, e->req, e->req_len);
    }

    if (e->gatt == G_DONE && !l->ready && (e->smp == SMP_DONE || e->bonded)) link_ready(h, l);
    if (l->ready) init_tick(h, l, now);
}
