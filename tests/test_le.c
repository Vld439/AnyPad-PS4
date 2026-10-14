/* End-to-end runs of the LE half of the host against the simulated LE
 * controller and pad. */
#include "sim_le.h"
#include "../src/host.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

typedef struct {
    int connects, disconnects, states, slot;
    pad_info info;
    pad_state last;
} seen;

static void on_connect(void *ud, int slot, const pad_info *info)
{
    seen *s = ud;
    s->connects++;
    s->slot = slot;
    s->info = *info;
}

static void on_state(void *ud, int slot, const pad_state *st)
{
    seen *s = ud;
    (void)slot;
    s->states++;
    s->last = *st;
}

static void on_disconnect(void *ud, int slot)
{
    seen *s = ud;
    (void)slot;
    s->disconnects++;
}

static const char *DB = "build/test_le.db";

static void run(host_t *h, int ms)
{
    long end = sim_le_now() + ms;
    while (sim_le_now() < end) host_poll(h, 5);
}

static host_t *open_host(sim_le_t *sim, seen *s)
{
    host_events ev = { on_connect, on_state, on_disconnect, s, NULL };
    return host_open(sim_le_hci(sim), DB, &ev);
}

/* An Xbox Series controller: report 1 is 16 bytes after its id. A is
 * pressed, Share too, the left stick is pushed right. */
static void xbox_pad(sim_le_pad *p, int sc, int rpa)
{
    static const uint8_t addr[6] = { 0x51, 0x52, 0x53, 0x54, 0x55, 0xD6 };
    static const uint8_t report[16] = {
        0xFF, 0xFF, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,     /* sticks */
        0x00, 0x00, 0x00, 0x00,                             /* triggers */
        0x00,                                               /* hat */
        0x01, 0x00, 0x01,                                   /* A; Share */
    };
    int i;

    memset(p, 0, sizeof *p);
    memcpy(p->addr, addr, 6);
    p->use_rpa = rpa;
    for (i = 0; i < 16; i++) p->irk[i] = (uint8_t)(0x90 + i);
    p->sc = sc;
    p->vid = 0x045E;
    p->pid = 0x0B13;
    p->map[0] = 0x05;                   /* never read: the profile knows the pad */
    p->map_len = 1;
    memcpy(p->report, report, sizeof report);
    p->report_len = sizeof report;
    p->rid_in = 1;
    p->rid_out = 3;
    p->mtu = 185;
}

static void test_xbox_sc(void)
{
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;
    uint8_t out[16];

    printf("Xbox Series over LE: Secure Connections pairing, GATT, reports, rumble\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 1, 0);
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 8000);

    CHECK(sim_le_error(sim) == NULL);
    if (sim_le_error(sim)) printf("  pad says: %s\n", sim_le_error(sim));
    CHECK(sim_le_paired(sim));
    CHECK(sim_le_encrypted(sim));
    CHECK(s.connects == 1);
    CHECK(s.info.vid == 0x045E && s.info.pid == 0x0B13);
    CHECK(s.info.profile && strcmp(s.info.profile, "Xbox") == 0);
    CHECK(s.states > 0);
    CHECK((s.last.buttons & 0x4000) && (s.last.buttons & 0x1));     /* cross, create */
    CHECK(s.last.lx == 0xFF && s.last.ly == 0x80);
    CHECK(host_paired_count(h) == 1);

    {
        pad_output o = { 255, 51, 0, 0, 0, 1 };
        CHECK(host_set_output(h, s.slot, &o));
        CHECK(sim_le_outputs(sim) == 1);
        CHECK(sim_le_last_output(sim, out, sizeof out) == 8);
        CHECK(out[0] == 0x0F && out[3] == 100 && out[4] == 20 && out[5] == 0xFF);
    }

    sim_le_power_off(sim);
    run(h, 200);
    CHECK(s.disconnects == 1);
    host_close(h);
    sim_le_free(sim);
}

static void test_reconnect_rpa(void)
{
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;

    printf("LE pad behind private addresses: paired, off, on again, back with its keys\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 1, 1);
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 10);
    run(h, 8000);
    CHECK(s.connects == 1 && sim_le_pairings(sim) == 1);

    sim_le_power_off(sim);
    run(h, 3000);                       /* the pairing window closes */
    sim_le_power_on(sim);               /* a new private address */
    run(h, 8000);
    CHECK(sim_le_error(sim) == NULL);
    if (sim_le_error(sim)) printf("  pad says: %s\n", sim_le_error(sim));
    CHECK(s.connects == 2);
    CHECK(sim_le_pairings(sim) == 1);   /* no second pairing: the stored LTK */
    CHECK(sim_le_encrypted(sim));
    CHECK(s.last.buttons & 0x4000);

    /* And after the host restarts, from the store. */
    host_close(h);
    sim_le_power_off(sim);
    memset(&s, 0, sizeof s);
    h = open_host(sim, &s);
    sim_le_power_on(sim);
    run(h, 8000);
    CHECK(s.connects == 1 && sim_le_pairings(sim) == 1);
    host_close(h);
    sim_le_free(sim);
}

/* An Xbox Series as it really behaves: it takes the bond as unfinished, and
 * keeps blinking with no input, until the host hands over its own identity;
 * and it sends nothing until it is told to leave suspend. With Secure
 * Connections and with legacy pairing, then back after being switched off. */
static void xbox_identity_once(int sc)
{
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;

    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, sc, 1);
    pad.wants_identity = 1;
    pad.needs_wake = 1;
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 10);
    run(h, 8000);
    CHECK(sim_le_error(sim) == NULL);
    if (sim_le_error(sim)) printf("  pad says: %s\n", sim_le_error(sim));
    CHECK(sim_le_paired(sim));
    CHECK(sim_le_identity(sim));
    CHECK(s.connects == 1);
    CHECK(s.states > 0 && (s.last.buttons & 0x4000));

    sim_le_power_off(sim);
    run(h, 3000);
    memset(&s, 0, sizeof s);
    sim_le_power_on(sim);               /* back with its keys: woken again */
    run(h, 8000);
    CHECK(sim_le_error(sim) == NULL);
    CHECK(s.connects == 1 && sim_le_pairings(sim) == 1);
    CHECK(s.states > 0 && (s.last.buttons & 0x4000));
    host_close(h);
    sim_le_free(sim);
}

static void test_xbox_identity(void)
{
    printf("Xbox Series that wants our identity and a wake-up: paired, input, back again\n");
    xbox_identity_once(1);
    xbox_identity_once(0);
}

/* A pad that does not ask for our identity is not given it. */
static void test_identity_only_if_asked(void)
{
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;

    printf("LE pad that does not take our identity: none is sent, input as before\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 1, 0);
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 10);
    run(h, 8000);
    CHECK(sim_le_error(sim) == NULL);
    CHECK(!sim_le_identity(sim));
    CHECK(s.states > 0 && (s.last.buttons & 0x4000));
    host_close(h);
    sim_le_free(sim);
}

/* Someone else's LE connection attempt is pending in the controller (the
 * system's, or one an earlier run left): every LE Create Connection is
 * refused with 0x0c until it is cancelled. Once, then several times. */
static void busy_once(int busy, long ms, int max_creates)
{
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;

    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 1, 0);
    pad.busy = busy;
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, ms);
    CHECK(sim_le_error(sim) == NULL);
    CHECK(s.connects == 1);
    CHECK(s.states > 0 && (s.last.buttons & 0x4000));
    CHECK(sim_le_cancels(sim) >= busy);
    CHECK(sim_le_creates(sim) <= max_creates);      /* no flood of attempts */
    printf("  busy %d: %d tries, %d cancels\n", busy, sim_le_creates(sim), sim_le_cancels(sim));
    host_close(h);
    sim_le_free(sim);
}

static void test_busy_controller(void)
{
    printf("LE Create Connection refused (0x0c): cancelled, tried again, connected\n");
    busy_once(1, 8000, 3);
    busy_once(3, 10000, 5);
    busy_once(5, 20000, 8);                         /* gives up, rests, then connects */
}

/* A pad no profile knows, with legacy pairing only and a 23-byte ATT MTU,
 * so its report map comes in several blobs. */
static const uint8_t k_desc[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
    0x85, 0x01,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x04,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x07, 0x75, 0x04, 0x95, 0x01, 0x09, 0x39, 0x81, 0x42,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
    0x05, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02,
    0x09, 0xC5, 0x09, 0xC4, 0x81, 0x02,
    0xC0,
};

static void test_generic_legacy(void)
{
    static const uint8_t report[9] = { 0x00, 0x80, 0x80, 0x80, 0x06, 0x02, 0x00, 0xFF, 0x00 };
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;

    printf("unknown LE pad: legacy pairing, report map in blobs, generic profile\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 0, 0);
    pad.vid = 0x2DC8;
    pad.pid = 0x9021;
    memcpy(pad.map, k_desc, sizeof k_desc);
    pad.map_len = sizeof k_desc;
    memcpy(pad.report, report, sizeof report);
    pad.report_len = sizeof report;
    pad.mtu = 23;
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 30);
    run(h, 10000);
    CHECK(sim_le_error(sim) == NULL);
    if (sim_le_error(sim)) printf("  pad says: %s\n", sim_le_error(sim));
    CHECK(sim_le_paired(sim));
    CHECK(sim_le_blob_reads(sim) >= 3);
    CHECK(s.connects == 1);
    CHECK(s.info.profile && strcmp(s.info.profile, "Generic HID") == 0);
    CHECK(s.last.lx == 0 && (s.last.buttons & 0x80));   /* hat west: left */
    CHECK(s.last.buttons & 0x2000);                      /* button 2: circle */
    CHECK(s.last.l2 == 255 && (s.last.buttons & 0x100)); /* brake */
    host_close(h);

    /* Legacy bonds reconnect with the distributed LTK, EDIV and Rand. */
    sim_le_power_off(sim);
    memset(&s, 0, sizeof s);
    h = open_host(sim, &s);
    sim_le_power_on(sim);
    run(h, 8000);
    CHECK(s.connects == 1 && sim_le_pairings(sim) == 1);
    CHECK(sim_le_error(sim) == NULL);
    host_close(h);
    sim_le_free(sim);
}

static void test_garbage(void)
{
    static const unsigned cids[] = { 0x0004, 0x0005, 0x0006, 0x0040 };
    static const uint8_t codes[] = { 0x05, 0x08, 0x0E, 0x0F, 0x13, 0x30, 0x3E };
    unsigned seed = 4242;
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;
    int i;

    printf("garbage ATT, SMP, signaling and LE events on a working LE link\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 1, 0);
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 30);
    run(h, 8000);
    CHECK(s.connects == 1);
    for (i = 0; i < 20000; i++) {
        uint8_t junk[300];
        int n, k;
        seed = seed * 1103515245u + 12345u;
        n = (int)((seed >> 8) % 280);
        for (k = 0; k < n; k++) {
            seed = seed * 1103515245u + 12345u;
            junk[k] = (uint8_t)(seed >> 16);
        }
        if (i % 4 == 3) sim_le_inject_event(sim, codes[(seed >> 4) % sizeof codes], junk, n < 255 ? n : 255);
        else sim_le_inject_acl(sim, cids[(seed >> 4) % 4], junk, n);
        if (i % 16 == 0) host_poll(h, 1);
    }
    run(h, 500);
    printf("  connects %d, disconnects %d, connected now %d\n", s.connects, s.disconnects,
           host_connected(h, s.slot));
    /* A garbage Disconnection Complete for our handle ends the link, and the
     * paired pad then reconnects: the count must simply add up. */
    CHECK(host_connected(h, s.slot) == (s.connects - s.disconnects == 1));
    host_close(h);
    sim_le_free(sim);
}

static int pair_once(int drop_every, unsigned seed)
{
    sim_le_pad pad;
    sim_le_t *sim;
    host_t *h;
    seen s;
    int ok;

    unlink(DB);
    memset(&s, 0, sizeof s);
    xbox_pad(&pad, 1, 0);
    pad.drop_every = drop_every;
    pad.seed = seed;
    sim = sim_le_new(&pad);
    h = open_host(sim, &s);
    if (!h) {
        sim_le_free(sim);
        return 0;
    }
    host_pair(h, 60);
    run(h, 45000);
    ok = s.connects >= 1 && s.states > 0 && (s.last.buttons & 0x4000);
    if (!ok) printf("  seed %u: connects %d states %d pairings %d error %s\n", seed, s.connects,
                    s.states, sim_le_pairings(sim), sim_le_error(sim) ? sim_le_error(sim) : "-");
    host_close(h);
    sim_le_free(sim);
    return ok;
}

static void test_stress(void)
{
    int ok = 0, runs = 30;
    unsigned seed;

    printf("LE pairing under 1-in-10 packet loss, %d runs\n", runs);
    for (seed = 1; seed <= (unsigned)runs; seed++) ok += pair_once(10, seed * 104729);
    printf("  %d/%d runs ended with a working pad\n", ok, runs);
    if (ok < runs) g_fail++;
}

int main(void)
{
    test_xbox_sc();
    test_reconnect_rpa();
    test_xbox_identity();
    test_identity_only_if_asked();
    test_busy_controller();
    test_generic_legacy();
    test_garbage();
    test_stress();
    printf(g_fail ? "%d check(s) failed\n" : "all LE checks passed\n", g_fail);
    return g_fail != 0;
}
