/* DualSense over the console's own Bluetooth. The driver protocol, the kernel
 * allowlist patch and the 0x31 output report come from OneFamily
 * (github.com/ChiricoG/OneFamily, MIT; see BT-PLAN.md), ported to AnyPad's
 * virtual pads. */

#include "bt_pad.h"
#include "bt_proto.h"
#include "log.h"
#include "profiles.h"
#include "ps4_vpad.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/syscall.h>

#if defined(__has_include)
#if __has_include(<ps4/kernel.h>)
#include <ps4/kernel.h>
#endif
#endif

/* Sony's DualSense, plain and Edge. */
#define SONY_VID 0x054C
#define DS5_PID 0x0CE6
#define EDGE_PID 0x0DF2

#define BT_MAX_DEVS 4

/* Syscall 11 is the console's kexec (run a function in kernel mode); it is not
 * in the FreeBSD numbering the SDK carries. */
#define SYS_KEXEC 11

/* What the driver's own ioctls take; the size is in the request's high bits. */
#define IOCTL_BT_REGISTER 0x8004622BUL
#define IOCTL_BT_UNREGISTER 0x8004622CUL
#define IOCTL_BT_GET_REPORT 0x80206233UL
#define IOCTL_BT_SEND_REPORT 0x80206232UL

/* A pad that stops sending is held this long before its slot goes; and it is
 * fed neutral at 60 Hz so the console keeps the port. */
#define BT_AWAY_MS 3000
#define BT_GRACE_MS 20000
#define BT_FEED_MS 16

/* How long a pad must be quiet before its report is fetched directly (the
 * driver does not always wake the kqueue after a feature report). */
#define BT_QUIET_MS 16

/* Output: the first report right away (it ends the pairing animation and the
 * pad starts sending 0x31), then a heartbeat. */
#define BT_OUT_FIRST_MS 20
#define BT_OUT_EVERY_MS 2500

/* ---- kernel side ---------------------------------------------------------- */

/* What the kernel passes a kexec function: the function and its arguments. */
typedef struct {
    void *fn;
    long arg1, arg2, arg3, arg4, arg5;
} kexec_args;

/* The four functions compare a device's vendor and product id against the
 * Thrustmaster and Guillemot entries; swapping those for Sony's lets the
 * DualSense through. Offsets from this console's 13.52 dump, in pairs. */
typedef struct {
    int restore;
    uint32_t fw;
    int rc;                     /* 0, or what failed */
} bt_patch_req;

typedef struct {
    uint64_t address;
    int restore;
    int rc;                     /* 0, or what failed */
    uint32_t original;          /* the capacity before the patch */
} bt_capacity_req;

static int bt_kernel_patch(void *td, void *args);

/* Patches (or restores) the driver's allowlist. Returns 0 and sets *rc, or a
 * negative errno from the syscall itself. */
static int bt_kernel_run(int (*fn)(void *, void *), void *arg)
{
    return (int)syscall(SYS_KEXEC, fn, arg, 0, 0, 0, 0);
}

/* The list patch. Runs in kernel mode: no calls, no libc, small stack. */
static int bt_kernel_patch(void *td, void *args)
{
    static const uint32_t off[12] = {
        0x59C869, 0x59C8C0,       /* pair 0 */
        0x5B1616, 0x5B1646,       /* pair 1 */
        0x5B1E86, 0x5B1EB9,       /* pair 2 */
        0x5B5C48, 0x5B64DE,       /* pair 3 */
        0x5B5FFD, 0x5B656E,       /* pair 4 */
        0x5B607B, 0x5B348A,       /* pair 5 */
    };
    kexec_args *ka = (kexec_args *)args;
    bt_patch_req *r = (bt_patch_req *)ka->arg1;
    uint64_t base, flags, cr0;
    uint32_t lo, hi;
    int i, original = 0, patched = 0;

    (void)td;

    /* The kernel's base is the syscall entry point minus its prologue. */
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000082));
    base = ((uint64_t)hi << 32 | lo) - 0x1C0;
    if ((base >> 48) != 0xFFFF || (base & 0x3FFF)) { r->rc = 0xE1; return 22; }
    if (r->fw != 0x1352) { r->rc = 0xE2; return 22; }
    /* The signature OneFamily checks on this firmware. 0x5B1616 is one of the
     * words the patch rewrites (0x044F -> 0x054C), so both are accepted: the
     * per-offset check below tells a patched word from an original one, and
     * that is also how a patch left by another payload is recognised. */
    {
        uint32_t sig = *(volatile uint32_t *)(base + 0x5B1616);

        if (sig != 0x0000044F && sig != 0x0000054C) { r->rc = 0xE3; return 22; }
    }

    for (i = 0; i < 12; i++) {
        uint32_t v = *(volatile uint32_t *)(base + off[i]);
        uint32_t want = (i & 1) ? 0x1000 : 0x044F;
        uint32_t done = (i & 1) ? 0x0CE6 : 0x054C;

        if (v == want) original++;
        else if (v == done) patched++;
        else { r->rc = 0x100 + (uint32_t)i; return 22; }
    }

    /* All in one state already: nothing to write, and remember which. */
    if (!r->restore ? patched == 12 : original == 12) {
        r->rc = (int)(patched << 8 | original);
        return 0;
    }
    /* Half patched: another payload did it, or died doing it. Do not guess. */
    if (original && patched) { r->rc = (int)(0x8000 | patched << 8 | original); return 22; }

    __asm__ volatile("pushfq; popq %0; cli; mov %%cr0, %1" : "=r"(flags), "=r"(cr0) :: "memory");
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0 & ~(1UL << 16)) : "memory");
    for (i = 0; i < 12; i++) {
        uint32_t want = (i & 1) ? 0x1000 : 0x044F;
        uint32_t done = (i & 1) ? 0x0CE6 : 0x054C;

        *(volatile uint32_t *)(base + off[i]) = r->restore ? want : done;
    }
    __asm__ volatile("mov %0, %%cr0; pushq %1; popfq" :: "r"(cr0), "r"(flags) : "memory", "cc");

    r->rc = 0;
    return 0;
}

static int bt_kernel_capacity(void *td, void *args);

/* Raises (or gives back) one link's output report capacity. Runs in kernel
 * mode: the bthid context is a pointer in the kernel's data. */
static int bt_kernel_capacity(void *td, void *args)
{
    kexec_args *ka = (kexec_args *)args;
    bt_capacity_req *r = (bt_capacity_req *)ka->arg1;
    uint64_t base;
    uint32_t lo, hi, o;

    (void)td;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000082));
    base = ((uint64_t)hi << 32 | lo) - 0x1C0;
    if ((base >> 48) != 0xFFFF || (base & 0x3FFF)) { r->rc = 0xE1; return 22; }
    if (!r->address || (r->address >> 48)) { r->rc = 0xE2; return 22; }

    /* The links: a context pointer at a fixed place, the array of links
     * after it, one every 0x1588 bytes, the pad's address at +0x30. */
    {
        uint8_t *ctx = *(uint8_t **)(base + 0x2612550);

        if (!ctx || ((uint64_t)ctx >> 48) != 0xFFFF) { r->rc = 0xE3; return 22; }
        for (o = 0x42A8; o < 0x185A0; o += 0x1588) {
            uint8_t *link = ctx + o;
            uint8_t *dev;
            volatile uint32_t *capacity;

            if (*(volatile uint64_t *)(link + 0x30) != r->address) continue;

            dev = *(uint8_t **)(link + 0x638);
            if (!dev || ((uint64_t)dev >> 48) != 0xFFFF) { r->rc = 0xE4; return 22; }
            if (*(volatile uint16_t *)(dev + 0x28) != SONY_VID ||
                (*(volatile uint16_t *)(dev + 0x2A) != DS5_PID &&
                 *(volatile uint16_t *)(dev + 0x2A) != EDGE_PID)) { r->rc = 0xE5; return 22; }

            capacity = (volatile uint32_t *)(link + 0x1574);
            if (*capacity != 22 && *capacity != 49 && *capacity != 78) { r->rc = 0xE6; return 22; }
            if (r->restore) {
                if (r->original != 22 && r->original != 49 && r->original != 78) {
                    r->rc = 0xE7;
                    return 22;
                }
                *capacity = r->original;
            } else {
                r->original = *capacity;
                *capacity = 78;
            }
            r->rc = 0;
            return 0;
        }
    }

    r->rc = 0xE8;                       /* no link with that address yet */
    return 22;
}

/* ---- the driver's interface ----------------------------------------------- */

struct bt_pair {
    uint16_t vid;
    uint16_t pid;
};

struct bt_get {
    uint64_t address;
    void *buf;
    uint64_t capacity;
    uint64_t reserved;
};

struct bt_send {
    uint64_t address;
    const void *buf;
    uint64_t len;
    uint64_t report;
};

/* FreeBSD's kevent on this console, spelled out: the SDK's headers are the
 * Linux-shaped ones for some of these. */
struct bt_kevent {
    uintptr_t ident;
    short filter;
    unsigned short flags;
    unsigned int fflags;
    intptr_t data;
    void *udata;
};

struct bt_timespec {
    long sec;
    long nsec;
};

#define BT_FILTER_READ (-1)
#define BT_EV_ADD 0x0001

/* ---- pads ----------------------------------------------------------------- */

typedef struct {
    uint64_t address;           /* 0: free entry */
    int slot;                   /* virtual pad slot, -1 while free */
    int live;                   /* the virtual pad was asked for */
    int connected;              /* reports are arriving */
    long last_input;
    long away_since;
    long last_feed;
    long last_out;
    int extended;               /* 0x31 reports are arriving */
    int out_started;            /* the pad took an output report */
    int out_errors;
    int out_unsupported;
    int capacity;               /* the link's capacity was raised */
    uint32_t capacity_original;
    char name[24];
    bt_out out;
    uint8_t raw[BT_REPORT_MAX];
    int raw_len;
    pad_state st;
} bt_dev;

struct bt_pad_mgr_s {
    int fd;
    int kq;
    int patched;                /* we patched the driver's list */
    int inherited;              /* it was patched before we started */
    int reg_ds5, reg_edge;
    uint32_t fw;
    bt_dev devs[BT_MAX_DEVS];
    bt_pad_feed_fn on_feed;
    bt_pad_attach_fn on_attach;
    bt_pad_detach_fn on_detach;
};

static uint32_t fw_bcd(void)
{
#if defined(__has_include)
#if __has_include(<ps4/kernel.h>)
    return kernel_get_fw_version() >> 16;
#endif
#endif
    return 0;
}

static bt_dev *dev_find(bt_pad_mgr_t *m, uint64_t address)
{
    int i;

    for (i = 0; i < BT_MAX_DEVS; i++)
        if (m->devs[i].address == address) return &m->devs[i];
    return NULL;
}

/* A free entry for a pad seen for the first time: an unused one, or the one
 * that has been away longest. */
static bt_dev *dev_acquire(bt_pad_mgr_t *m, uint64_t address)
{
    bt_dev *d = dev_find(m, address);
    bt_dev *oldest = NULL;
    int i;

    if (d) return d;
    for (i = 0; i < BT_MAX_DEVS; i++) {
        if (!m->devs[i].address) {
            d = &m->devs[i];
            memset(d, 0, sizeof *d);
            d->slot = -1;
            d->address = address;
            return d;
        }
        if (m->devs[i].away_since &&
            (!oldest || m->devs[i].away_since < oldest->away_since))
            oldest = &m->devs[i];
    }
    if (!oldest) return NULL;

    log_line("bt_pad: %012llx: no free entry, releasing the one away longest", (unsigned long long)address);
    if (oldest->live && m->on_detach) m->on_detach(oldest->slot);
    memset(oldest, 0, sizeof *oldest);
    oldest->slot = -1;
    oldest->address = address;
    return oldest;
}

static void dev_attach(bt_pad_mgr_t *m, bt_dev *d)
{
    int slot, i;

    /* The highest slot nobody else holds; vpad_add() reserves it, so a pad
     * the console has not announced yet is not taken twice. */
    for (slot = MAX_PADS - 1; slot >= 0; slot--) {
        int taken = 0;

        for (i = 0; i < BT_MAX_DEVS; i++)
            if (&m->devs[i] != d && m->devs[i].slot == slot) taken = 1;
        if (!taken && vpad_add(slot)) break;
    }
    if (slot < 0) {
        log_line("bt_pad: %012llx: every virtual pad slot is in use", (unsigned long long)d->address);
        return;
    }

    d->slot = slot;
    d->live = 1;
    snprintf(d->name, sizeof d->name, "DualSense (Bluetooth)");
    if (m->on_attach) m->on_attach(slot, d->name);
    log_line("bt_pad: %012llx: %s on slot %d", (unsigned long long)d->address, d->name, slot);
}

static void dev_release(bt_pad_mgr_t *m, bt_dev *d)
{
    if (d->live && m->on_detach) m->on_detach(d->slot);
    memset(d, 0, sizeof *d);
    d->slot = -1;
}

/* ---- output --------------------------------------------------------------- */

static int bt_capacity(bt_dev *d, int restore)
{
    bt_capacity_req req;

    memset(&req, 0, sizeof req);
    req.address = d->address;
    req.restore = restore ? 1 : 0;
    req.original = restore ? d->capacity_original : 0;

    if (bt_kernel_run(bt_kernel_capacity, &req) != 0) {
        log_line("bt_pad: %012llx: output capacity %s refused: rc 0x%x",
                 (unsigned long long)d->address, restore ? "restore" : "raise", req.rc);
        return -1;
    }
    if (!restore) d->capacity_original = req.original;
    return 0;
}

static void dev_output(bt_pad_mgr_t *m, bt_dev *d, const pad_output *out)
{
    uint8_t buf[78];
    struct bt_send req;
    long rc;

    if (!d->connected || d->out_unsupported) return;

    /* The driver sizes the link for a 22-byte pad; a 78-byte report through
     * it makes the DualSense drop the link. */
    if (!d->capacity) {
        if (bt_capacity(d, 0) != 0) {
            d->out_unsupported = 1;
            log_line("bt_pad: %012llx: no output until it reconnects (capacity)", (unsigned long long)d->address);
            return;
        }
        d->capacity = 1;
    }

    if (bt_output_build(&d->out, out, buf, sizeof buf) == 0) return;

    memset(&req, 0, sizeof req);
    req.address = d->address;
    req.buf = buf;
    req.len = 78;
    req.report = 0x31;
    rc = syscall(SYS_ioctl, m->fd, IOCTL_BT_SEND_REPORT, &req, 0, 0, 0);
    if (rc == 0) {
        d->out_errors = 0;
        d->out_started = 1;
        d->last_out = now_ms();
    } else if ((uint32_t)rc != 0x802F3707U) {
        if (++d->out_errors >= 3) {
            d->out_unsupported = 1;
            log_line("bt_pad: %012llx: output reports refused (0x%lx), off", (unsigned long long)d->address, (unsigned long)rc);
        }
    }
}

void bt_pad_output(bt_pad_mgr_t *m, int slot, const pad_output *out)
{
    int i;

    if (!m || !out) return;
    for (i = 0; i < BT_MAX_DEVS; i++) {
        bt_dev *d = &m->devs[i];

        if (d->address && d->slot == slot && d->connected) {
            dev_output(m, d, out);
            return;
        }
    }
}

/* ---- input ---------------------------------------------------------------- */

static void dev_deliver(bt_pad_mgr_t *m, bt_dev *d, const uint8_t *rep, int len, long now)
{
    pad_state st;

    pad_state_reset(&st);
    if (ds5_bt_parse(rep, len, &st) == PARSE_NONE) return;

    if (!d->live) {
        dev_attach(m, d);
        if (!d->live) return;
    }

    if (rep[0] == 0x31 && !d->extended) {
        d->extended = 1;
        log_line("bt_pad: %012llx: extended reports", (unsigned long long)d->address);
    }
    if (!d->connected) {
        d->connected = 1;
        d->away_since = 0;
        log_line("bt_pad: %012llx: sending, slot %d", (unsigned long long)d->address, d->slot);
    }

    d->last_input = now;
    d->st = st;
    if (m->on_feed) m->on_feed(d->slot, &st);
}

/* Fetches this address's latest report. `fresh` says an event announced it, so
 * the first read counts even if it is the report already seen. */
static void dev_drain(bt_pad_mgr_t *m, uint64_t address, long now, int fresh)
{
    int i;

    for (i = 0; i < 8; i++) {
        uint8_t buf[BT_REPORT_MAX];
        struct bt_get req;
        const uint8_t *rep;
        bt_dev *d;
        long n;
        int len;

        memset(&req, 0, sizeof req);
        req.address = address;
        req.buf = buf;
        req.capacity = sizeof buf;
        n = syscall(SYS_ioctl, m->fd, IOCTL_BT_GET_REPORT, &req, 0, 0, 0);
        if (n <= 0 || n > (long)sizeof buf) return;

        d = dev_find(m, address);
        /* The same bytes again: the driver keeps the last report. */
        if (d && (!fresh || i > 0) && n == d->raw_len && memcmp(buf, d->raw, (size_t)n) == 0)
            return;

        len = bt_report_unwrap(buf, (int)n, &rep);
        if (!len || !bt_input_ok(rep, len)) continue;

        d = dev_acquire(m, address);
        if (!d) {
            static int said;
            if (!said) { said = 1; log_line("bt_pad: %d DualSense in use, another ignored", BT_MAX_DEVS); }
            return;
        }
        memcpy(d->raw, buf, (size_t)n);
        d->raw_len = (int)n;
        dev_deliver(m, d, rep, len, now);
    }
}

/* ---- polling -------------------------------------------------------------- */

static void dev_timeouts(bt_pad_mgr_t *m, long now)
{
    int i;

    for (i = 0; i < BT_MAX_DEVS; i++) {
        bt_dev *d = &m->devs[i];
        pad_state st;

        if (!d->address) continue;

        if (d->away_since) {
            if (now - d->away_since < BT_GRACE_MS) continue;
            log_line("bt_pad: %012llx: away %ld s, slot %d released",
                     (unsigned long long)d->address, (now - d->last_input) / 1000, d->slot);
            dev_release(m, d);
            continue;
        }
        if (now - d->last_input < BT_AWAY_MS) continue;

        /* Quiet: neutral to the console, but keep the slot for a while. */
        pad_state_reset(&st);
        d->connected = 0;
        d->away_since = now;
        d->st = st;
        if (d->live && m->on_feed) m->on_feed(d->slot, &st);
        log_line("bt_pad: %012llx: quiet, slot %d held", (unsigned long long)d->address, d->slot);
    }
}

static void dev_keepalive(bt_pad_mgr_t *m, long now)
{
    pad_output neutral = { 0, 0, 0, 112, 209, 1 };    /* PlayStation blue */
    int i;

    for (i = 0; i < BT_MAX_DEVS; i++) {
        bt_dev *d = &m->devs[i];
        long every;

        if (!d->address || !d->connected) continue;

        every = d->out_started ? BT_OUT_EVERY_MS : BT_OUT_FIRST_MS;
        if (d->last_out && now - d->last_out < every) continue;

        /* Neutral: nothing of a game's rumble reaches AnyPad's pads yet. */
        neutral.player = (uint8_t)(MAX_PADS - d->slot);
        dev_output(m, d, &neutral);
        d->last_out = now;
    }
}

/* Keeps the virtual pad fed with its last state while no report comes, so the
 * console does not treat it as gone. */
static void dev_feed_idle(bt_pad_mgr_t *m, long now)
{
    int i;

    for (i = 0; i < BT_MAX_DEVS; i++) {
        bt_dev *d = &m->devs[i];

        if (!d->address || !d->live || !m->on_feed) continue;
        if (d->connected && now - d->last_input < 500) continue;
        if (d->last_feed && now - d->last_feed < BT_FEED_MS) continue;
        d->last_feed = now;
        m->on_feed(d->slot, &d->st);
    }
}

static int bt_watch(bt_pad_mgr_t *m)
{
    struct bt_kevent change;

    memset(&change, 0, sizeof change);
    change.ident = (uintptr_t)m->fd;
    change.filter = BT_FILTER_READ;
    change.flags = BT_EV_ADD;
    return syscall(SYS_kevent, m->kq, &change, 1, 0, 0, 0) == 0;
}

static int bt_ready(bt_pad_mgr_t *m)
{
    struct bt_kevent event;
    struct bt_timespec timeout;

    memset(&event, 0, sizeof event);
    memset(&timeout, 0, sizeof timeout);        /* do not wait */
    return syscall(SYS_kevent, m->kq, 0, 0, &event, 1, &timeout) > 0;
}

void bt_pad_poll(bt_pad_mgr_t *m, long now)
{
    uint8_t events[256];
    long n, i;

    if (!m) return;

    dev_timeouts(m, now);
    dev_keepalive(m, now);
    dev_feed_idle(m, now);

    if (bt_ready(m)) {
        n = read(m->fd, events, sizeof events);
        for (i = 0; i + BT_EVENT_LEN <= n; i += BT_EVENT_LEN) {
            uint64_t address;
            bt_dev *d;

            if (!bt_event_address(events + i, &address) || !address) continue;

            /* The link went down or came up: the pad starts over in 0x01. */
            if (events[i] == 0x18 || events[i] == 0x19) {
                d = dev_find(m, address);
                if (d) {
                    d->extended = 0;
                    d->out_started = 0;
                    d->out_errors = 0;
                    d->out_unsupported = 0;
                    d->capacity = 0;
                    d->out = (bt_out){ 0, 0 };
                }
            }
            dev_drain(m, address, now, 1);
        }
        return;
    }

    /* No event: ask for the pads that have gone quiet directly. */
    for (i = 0; i < BT_MAX_DEVS; i++) {
        bt_dev *d = &m->devs[i];

        if (d->address && now - d->last_input >= BT_QUIET_MS)
            dev_drain(m, d->address, now, 0);
    }
}

/* ---- start and stop ------------------------------------------------------- */

static int bt_register(bt_pad_mgr_t *m, uint16_t vid, uint16_t pid)
{
    struct bt_pair pair;

    pair.vid = vid;
    pair.pid = pid;
    return syscall(SYS_ioctl, m->fd, IOCTL_BT_REGISTER, &pair, 0, 0, 0) == 0;
}

static void bt_unregister(bt_pad_mgr_t *m, uint16_t vid, uint16_t pid)
{
    struct bt_pair pair;

    pair.vid = vid;
    pair.pid = pid;
    syscall(SYS_ioctl, m->fd, IOCTL_BT_UNREGISTER, &pair, 0, 0, 0);
}

bt_pad_mgr_t *bt_pad_init(bt_pad_feed_fn on_feed,
                          bt_pad_attach_fn on_attach,
                          bt_pad_detach_fn on_detach)
{
    bt_pad_mgr_t *m;
    uint64_t mask = 0x1FF000000ULL;
    bt_patch_req req;
    int i;

    m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->fd = m->kq = -1;
    m->on_feed = on_feed;
    m->on_attach = on_attach;
    m->on_detach = on_detach;
    for (i = 0; i < BT_MAX_DEVS; i++) m->devs[i].slot = -1;

    m->fw = fw_bcd();
    if (m->fw != 0x1352) {
        log_line("bt_pad: firmware 0x%04X not supported (only 13.52)", m->fw);
        free(m);
        return NULL;
    }

    m->fd = open("/dev/bt", O_RDWR);
    if (m->fd < 0) {
        log_line("bt_pad: open /dev/bt: %s", strerror(errno));
        free(m);
        return NULL;
    }

    m->kq = (int)syscall(SYS_kqueue, 0, 0, 0);
    if (m->kq < 0 || !bt_watch(m)) {
        log_line("bt_pad: kqueue: %s", strerror(errno));
        if (m->kq >= 0) close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }

    /* Everything the driver reports about controllers, from now on. */
    if (write(m->fd, &mask, sizeof mask) != (long)sizeof mask) {
        log_line("bt_pad: subscribe to /dev/bt: %s", strerror(errno));
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }

    memset(&req, 0, sizeof req);
    req.fw = m->fw;
    if (bt_kernel_run(bt_kernel_patch, &req) != 0) {
        log_line("bt_pad: allowlist patch failed: rc 0x%x", req.rc);
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }
    if (req.rc & 0x8000) {
        log_line("bt_pad: the allowlist is half patched by another payload (0x%x); left alone", req.rc);
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }
    /* All 12 already pointed at Sony's ids: another payload got here first. */
    if (req.rc & 0xFF00) {
        m->inherited = 1;
        log_line("bt_pad: the allowlist was already patched; it will be left as found");
    } else {
        m->patched = 1;
        log_line("bt_pad: allowlist patched for 054C:0CE6");
    }

    if (bt_register(m, SONY_VID, DS5_PID)) {
        m->reg_ds5 = 1;
    } else {
        log_line("bt_pad: register 054C:0CE6 refused: %s", strerror(errno));
        if (m->patched) {
            memset(&req, 0, sizeof req);
            req.restore = 1;
            req.fw = m->fw;
            bt_kernel_run(bt_kernel_patch, &req);
        }
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }
    /* The Edge answers to a product id of its own. */
    if (bt_register(m, SONY_VID, EDGE_PID)) {
        m->reg_edge = 1;
    } else {
        log_line("bt_pad: register 054C:0DF2 refused: %s", strerror(errno));
    }

    log_line("bt_pad: ready (firmware 0x%04X, up to %d pads)", m->fw, BT_MAX_DEVS);
    return m;
}

void bt_pad_close(bt_pad_mgr_t *m)
{
    int i;

    if (!m) return;

    /* The pads are still connected: give their links back first. */
    for (i = 0; i < BT_MAX_DEVS; i++)
        if (m->devs[i].address && m->devs[i].capacity)
            bt_capacity(&m->devs[i], 1);

    for (i = 0; i < BT_MAX_DEVS; i++)
        if (m->devs[i].live && m->on_detach) m->on_detach(m->devs[i].slot);

    if (m->reg_edge) bt_unregister(m, SONY_VID, EDGE_PID);
    if (m->reg_ds5) bt_unregister(m, SONY_VID, DS5_PID);

    if (m->patched) {
        bt_patch_req req;

        memset(&req, 0, sizeof req);
        req.restore = 1;
        req.fw = m->fw;
        /* rc says what happened in kernel space: the syscall itself returning
         * 0 only means the function ran. */
        if (bt_kernel_run(bt_kernel_patch, &req) != 0 || req.rc != 0)
            log_line("bt_pad: could not restore the allowlist: rc 0x%x", req.rc);
        else
            log_line("bt_pad: allowlist restored");
    }

    if (m->kq >= 0) close(m->kq);
    if (m->fd >= 0) close(m->fd);
    free(m);
}
