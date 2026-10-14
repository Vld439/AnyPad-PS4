/* Controllers we have no hardware for, emulated as their makers and the
 * projects that studied them describe them, against the real host stack:
 * does AnyPad find them, link them and read them? */
#include "sim.h"
#include "../src/host.h"
#include "../src/util.h"

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

static const char *DB = "build/test_compat.db";

static void run(host_t *h, int ms)
{
    long end = sim_now() + ms;
    while (sim_now() < end) host_poll(h, 5);
}

static host_t *open_host(sim_t *sim, seen *s)
{
    host_events ev = { on_connect, on_state, on_disconnect, s, NULL };
    return host_open(sim_hci(sim), DB, &ev);
}

static void base_pad(sim_pad *p, uint16_t vid, uint16_t pid, uint32_t cod)
{
    static const unsigned char addr[6] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 };

    memset(p, 0, sizeof *p);
    p->mode = SIM_WAIT_PAIRING;
    memcpy(p->addr, addr, 6);
    p->cod = cod;
    p->vid = vid;
    p->pid = pid;
}

/* ---- DualShock 4 clones -------------------------------------------------- */

/* Full report 0x11 (cross, dpad up, left stick 0x20/0xE0, battery full)
 * and short report 0x01 (cross only, sticks centred). */
static void ds4_reports(sim_pad *p)
{
    memset(p->report, 0, 78);
    p->report[0] = 0x11;
    p->report[1] = 0xC0;
    p->report[3] = 0x20; p->report[4] = 0xE0; p->report[5] = 0x80; p->report[6] = 0x80;
    p->report[7] = 0x20;
    p->report[32] = 0x08;
    p->report_len = 78;
    memset(p->basic, 0, 10);
    p->basic[0] = 0x01;
    p->basic[1] = p->basic[2] = p->basic[3] = p->basic[4] = 0x80;
    p->basic[5] = 0x20 | 0x08;          /* cross, hat centred */
    p->basic_len = 10;
}

/* A clone that answers to the DualShock 4's ids but stays in its short
 * report until its calibration (feature 0x05) is read: the light bar
 * report the original obeys does nothing. */
static void test_ds4_clone_needs_feature(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    printf("DualShock 4 clone that only wakes when its calibration is read\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0x054C, 0x05C4, 0x002508);
    ds4_reports(&pad);
    pad.needs_output = 1;
    pad.wake_feature = 0x05;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 20000);
    CHECK(s.connects == 1);
    CHECK(s.info.profile && strcmp(s.info.profile, "DualShock 4") == 0);
    CHECK(sim_feature_requests(sim) >= 1);
    CHECK(s.last.has_battery);              /* only the full report carries it */
    CHECK((s.last.buttons & 0x4000) && (s.last.buttons & 0x10));
    CHECK(s.last.lx == 0x20 && s.last.ly == 0xE0);
    printf("  %d feature request(s), %d output report(s)\n", sim_feature_requests(sim), sim_outputs(sim));
    host_close(h);
    sim_free(sim);
}

/* The original: wakes with the light bar report, and is never asked for
 * its calibration (its path stays as it was). */
static void test_ds4_original_untouched(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    printf("original DualShock 4: woken by the light bar report, no feature request\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0x054C, 0x09CC, 0x002508);
    ds4_reports(&pad);
    pad.needs_output = 1;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 30);
    run(h, 20000);
    CHECK(s.connects == 1);
    CHECK(s.last.has_battery);
    CHECK(sim_feature_requests(sim) == 0);
    host_close(h);
    sim_free(sim);
}

/* ---- clones that give no vendor and product, only a name ------------------- */

static void eir_named(sim_pad *p, const char *name)
{
    int n = (int)strlen(name);
    p->eir[0] = (unsigned char)(n + 1);
    p->eir[1] = 0x09;                   /* complete local name */
    memcpy(p->eir + 2, name, (size_t)n);
    p->eir_len = n + 2;
}

static void switch_reports(sim_pad *p)
{
    p->basic[0] = 0x3F; p->basic[3] = 8; p->basic_len = 12;
    memset(p->report, 0, 49);
    p->report[0] = 0x30; p->report[3] = 0x04;      /* B: cross */
    p->report[7] = p->report[10] = 8;
    p->report[8] = p->report[11] = 0x80;
    p->report_len = 49;
}

static void named_clone(const char *name, int ds4, const char *want)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0, 0, 0x002508);
    pad.no_device_id = 1;
    eir_named(&pad, name);
    if (ds4) ds4_reports(&pad); else switch_reports(&pad);
    pad.needs_output = 1;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 20000);
    if (want) {
        CHECK(s.connects == 1);
        CHECK(s.info.profile && strcmp(s.info.profile, want) == 0);
        CHECK(s.states > 0 && (s.last.buttons & 0x4000));       /* cross, from the full report */
    } else {
        CHECK(!(s.info.profile && (strcmp(s.info.profile, "DualShock 4") == 0 || strcmp(s.info.profile, "Switch Pro") == 0)));
    }
    printf("  \"%s\": %s\n", name, s.info.profile ? s.info.profile : "(no profile)");
    host_close(h);
    sim_free(sim);
}

static void test_named_clones(void)
{
    printf("clones with no Device ID, recognised by the name they announce\n");
    named_clone("Wireless Controller", 1, "DualShock 4");
    named_clone("Pro Controller", 0, "Switch Pro");
    named_clone("Gamepad 3000", 1, NULL);       /* an unknown name: not taken for either */
}

/* ---- single Joy-Con, held sideways ------------------------------------------ */

/* 12-bit stick values in the Switch's packing. */
static void stick12(unsigned char *d, int x, int y)
{
    d[0] = (unsigned char)(x & 0xFF);
    d[1] = (unsigned char)(((x >> 8) & 0x0F) | ((y & 0x0F) << 4));
    d[2] = (unsigned char)(y >> 4);
}

static void joycon_once(int right)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0x057E, right ? 0x2007 : 0x2006, 0x002508);
    pad.basic[0] = 0x3F; pad.basic_len = 12;
    memset(pad.report, 0, 49);
    pad.report[0] = 0x30;
    pad.report[2] = 0x80;               /* battery 4 of 4 */
    /* The stick rests in the middle for the first reports (its rest is measured). */
    stick12(pad.report + 6, 2048, 2048);
    stick12(pad.report + 9, 2048, 2048);
    pad.report_len = 49;
    pad.needs_output = 1;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 15000);
    CHECK(s.connects == 1);
    CHECK(s.info.profile && strcmp(s.info.profile, "Joy-Con") == 0);
    CHECK(s.last.lx > 120 && s.last.lx < 136 && s.last.ly > 120 && s.last.ly < 136);   /* centred */
    if (!right) {
        pad.report[5] = 0x08;           /* the left arrow: the bottom button held sideways */
        stick12(pad.report + 6, 3600, 2048);    /* pushed towards the pad's right: up, sideways */
    } else {
        pad.report[3] = 0x08;           /* A: the bottom button held sideways */
        stick12(pad.report + 9, 2048, 3600);    /* pushed towards the pad's top: right, sideways */
    }
    sim_set_report(sim, pad.report, pad.report_len);
    run(h, 300);
    CHECK(s.last.buttons & 0x4000);     /* cross */
    if (!right) CHECK(s.last.ly < 30 && s.last.lx > 120 && s.last.lx < 136);
    else CHECK(s.last.lx > 225 && s.last.ly > 120 && s.last.ly < 136);
    printf("  Joy-Con (%s): cross, stick %u/%u\n", right ? "R" : "L", s.last.lx, s.last.ly);
    host_close(h);
    sim_free(sim);
}

static void test_joycon(void)
{
    printf("single Joy-Con held sideways: buttons and stick turned a quarter\n");
    joycon_once(0);
    joycon_once(1);
}

/* ---- a pad that drops without a word and calls back ------------------------- */

static void test_silent_drop_then_call(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    printf("paired pad that drops without the host hearing of it, then calls back\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0x057E, 0x2009, 0x002508);
    switch_reports(&pad);
    pad.needs_output = 1;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 12000);
    CHECK(s.connects == 1);
    sim_vanish(sim);                    /* asleep, or out of range: no event reaches us */
    run(h, 1000);
    CHECK(s.disconnects == 0);          /* we still think it is there */
    sim_power_on(sim);                  /* it wakes and calls the console */
    run(h, 12000);
    CHECK(s.disconnects == 1);          /* the dead link let go ... */
    CHECK(s.connects == 2);             /* ... and the call taken */
    CHECK(sim_link_up(sim));
    s.states = 0;
    run(h, 500);
    CHECK(s.states > 0 && (s.last.buttons & 0x4000));       /* and it is read again */
    printf("  connects %d, disconnects %d\n", s.connects, s.disconnects);
    host_close(h);
    sim_free(sim);
}

/* ---- Nintendo Wii U Pro and Wii Remote ---------------------------------------- */

/* Wii U Pro extension bytes: sticks centred, nothing pressed (buttons are
 * low-active), battery full, not charging. */
static void wiiu_ext(unsigned char *e)
{
    int i;
    for (i = 0; i < 8; i += 2) { e[i] = 0x00; e[i + 1] = 0x08; }
    e[8] = 0xFF;
    e[9] = 0xFF;
    e[10] = 0xCF;
    memset(e + 11, 0, 8);
}

static sim_t *wii_pad(sim_pad *pad, uint16_t pid, uint32_t cod, const char *name, int wii, int pin)
{
    base_pad(pad, 0x057E, pid, cod);
    if (name) eir_named(pad, name);
    pad->legacy_pin = pin;
    pad->wii = wii;
    pad->basic[0] = pad->basic[1] = 0;
    if (wii == 1) wiiu_ext(pad->report);
    pad->report_len = 19;
    return sim_new(pad);
}

static void test_wiiu_pro(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    printf("Wii U Pro: the console's address as PIN, its extension woken, mode 0x34, and again after a status report\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    sim = wii_pad(&pad, 0x0330, 0x002508, "Nintendo RVL-CNT-01-UC", 1, 1);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 15000);
    CHECK(sim_pin_ok(sim));
    CHECK(s.connects == 1);
    CHECK(s.info.profile && strcmp(s.info.profile, "Wii") == 0);
    CHECK(s.last.has_battery && s.last.battery_pct == 100);
    CHECK(s.last.lx > 120 && s.last.lx < 136 && s.last.ly > 120 && s.last.ly < 136);
    pad.report[9] &= (unsigned char)~0x40;          /* B: bottom */
    pad.report[4] = 0x00; pad.report[5] = 0x03;     /* left stick pushed down (LY 0x300) */
    sim_set_report(sim, pad.report, 19);
    run(h, 300);
    CHECK(s.last.buttons & 0x4000);                  /* cross */
    CHECK(s.last.ly > 200);                          /* down */
    /* A status report: the pad goes quiet until its mode is set again. */
    sim_wii_status(sim);
    pad.report[9] = 0xFF & (unsigned char)~0x10;     /* now A: right */
    sim_set_report(sim, pad.report, 19);
    run(h, 2000);
    CHECK(s.last.buttons & 0x2000);                  /* circle: it reports again */
    printf("  PIN ok %d, %d PIN reply(ies)\n", sim_pin_ok(sim), sim_pins(sim));
    host_close(h);
    sim_free(sim);
}

static void test_wii_remote(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    printf("Wii Remote alone (its class says so, no name): address PIN, core buttons held sideways\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    sim = wii_pad(&pad, 0x0306, 0x002504, NULL, 2, 1);
    pad.basic[1] = 0x01;                             /* 2 */
    pad.basic[0] = 0x08;                             /* up on the remote: left, held sideways */
    sim_free(sim);
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 30);
    run(h, 15000);
    CHECK(sim_pin_ok(sim));
    CHECK(s.connects == 1 && s.info.profile && strcmp(s.info.profile, "Wii") == 0);
    CHECK((s.last.buttons & 0x4000) && (s.last.buttons & 0x80));     /* cross, left */
    host_close(h);
    sim_free(sim);
}

/* An old pad that wants "0000" still gets it; one that refuses it gets the
 * console's address the next time it is called. */
static void test_pins(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;

    printf("legacy PIN: \"0000\" kept for the pads that want it, the address after a refusal\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0x054C, 0x05C4, 0x002508);       /* an old DS4-like pad that pairs with "0000" */
    ds4_reports(&pad);
    pad.needs_output = 1;
    pad.legacy_pin = 2;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    host_pair(h, 30);
    run(h, 12000);
    printf("  old pad: %d PIN reply(ies), ok %d, link %d\n", sim_pins(sim), sim_pin_ok(sim), sim_link_up(sim));
    CHECK(sim_pin_ok(sim) && sim_pins(sim) == 1);
    CHECK(sim_link_up(sim));
    host_close(h);
    sim_free(sim);

    unlink(DB);
    memset(&s, 0, sizeof s);
    sim = wii_pad(&pad, 0x0330, 0x002508, NULL, 1, 1);  /* no name, a gamepad's class */
    h = open_host(sim, &s);
    host_pair(h, 60);
    run(h, 40000);
    CHECK(sim_pins(sim) >= 2);                       /* "0000" refused, then the address */
    CHECK(sim_pin_ok(sim));
    CHECK(s.connects == 1);
    printf("  unnamed Wii pad: %d PIN replies, ok %d\n", sim_pins(sim), sim_pin_ok(sim));
    host_close(h);
    sim_free(sim);
}

/* ---- a pad that links but sends nothing -------------------------------------- */

static void test_silent_pad_no_virtual(void)
{
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    seen s;
    unsigned char rep[49];

    printf("a pad that links but stays silent gets no virtual DualSense until it speaks\n");
    unlink(DB);
    memset(&s, 0, sizeof s);
    base_pad(&pad, 0x057E, 0x2009, 0x002508);
    pad.report_len = 0;                 /* the 8BitDo back from sleep: linked, mute */
    pad.basic_len = 0;
    sim = sim_new(&pad);
    h = open_host(sim, &s);
    CHECK(h != NULL);
    if (!h) return;
    host_pair(h, 30);
    run(h, 12000);
    CHECK(sim_link_up(sim));
    CHECK(s.connects == 0);             /* nothing for the console's user screen to wait on */
    memset(rep, 0, sizeof rep);
    rep[0] = 0x30; rep[3] = 0x04; rep[7] = rep[10] = 8; rep[8] = rep[11] = 0x80;
    sim_set_report(sim, rep, sizeof rep);
    run(h, 2000);
    CHECK(s.connects == 1);             /* it spoke: now it has one */
    CHECK(s.last.buttons & 0x4000);
    printf("  connects %d after it spoke\n", s.connects);
    host_close(h);
    sim_free(sim);
}

int main(void)
{
    test_ds4_clone_needs_feature();
    test_ds4_original_untouched();
    test_named_clones();
    test_joycon();
    test_silent_drop_then_call();
    test_wiiu_pro();
    test_wii_remote();
    test_pins();
    test_silent_pad_no_virtual();
    printf(g_fail ? "%d check(s) failed\n" : "all compatibility checks passed\n", g_fail);
    return g_fail != 0;
}
