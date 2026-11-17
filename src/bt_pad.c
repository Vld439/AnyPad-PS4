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
#define BT_TEST_MS 100              /* how often a rumble asked for by hand repeats */

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

/* The driver's list has room for six ids. Each one is a `cmp` against an
 * immediate in the kernel's text, and where those immediates are depends on
 * the firmware: the ones this has been seen on are in a table. A firmware
 * with no table is not guessed at: the text is read once, the comparisons of
 * the ids the list starts with (Thrustmaster's 044F:1000) are logged, and the
 * option says why it cannot go on. That log is all a table needs.
 *
 * The 9.00, 11.00 and 12.50 tables are OneFamily's (daemon/src/bt_driver.c,
 * MIT); 13.52's comes from this console's own dump. */
#define BT_PAIRS 6                  /* ids the driver's list has room for */
#define BT_KNOWN_PAIRS 12           /* ids a payload may have left there */
#define BT_SCAN_START 0x4E0000      /* where the comparisons live */
#define BT_SCAN_END 0x660000

typedef struct {
    int restore;
    uint32_t fw;
    uint16_t ids[BT_PAIRS][2];          /* in: the ids to write, cycled */
    int nids;
    uint16_t known[BT_KNOWN_PAIRS][2];  /* in: ids already put there */
    int nknown;
    uint32_t off[BT_PAIRS][2];          /* out: the pairs found, by scanning */
    int npairs;                         /* out */
    int dynamic;                        /* out: 1 when they were scanned */
    int rc;                             /* 0, or what happened */
} bt_patch_req;

typedef struct {
    uint32_t fw;
    int npairs;
    uint32_t off[BT_PAIRS][2];
} bt_fw_offsets;

static const bt_fw_offsets k_offsets[] = {
    { 0x0900, 5, { { 0x5A5E56, 0x5A5E86 }, { 0x5A66C6, 0x5A66F9 }, { 0x5AA498, 0x5AB08A },
                   { 0x5AA84D, 0x5AAD2E }, { 0x5AA8CB, 0x5AADBE } } },
    { 0x1100, 5, { { 0x5A4206, 0x5A4236 }, { 0x5A4A76, 0x5A4AA9 }, { 0x5A8838, 0x5A942A },
                   { 0x5A8BED, 0x5A90CE }, { 0x5A8C6B, 0x5A915E } } },
    { 0x1250, 5, { { 0x5B0DA6, 0x5B0DD6 }, { 0x5B1616, 0x5B1649 }, { 0x5B53D8, 0x5B5FCA },
                   { 0x5B578D, 0x5B5C6E }, { 0x5B580B, 0x5B5CFE } } },
    { 0x1352, 6, { { 0x59C869, 0x59C8C0 }, { 0x5B1616, 0x5B1646 }, { 0x5B1E86, 0x5B1EB9 },
                   { 0x5B5C48, 0x5B64DE }, { 0x5B5FFD, 0x5B656E }, { 0x5B607B, 0x5B348A } } },
};

typedef struct {
    uint64_t address;
    int restore;
    int rc;                     /* 0, or what failed */
    uint16_t vid, pid;          /* what the link has to be */
    uint32_t original;          /* the capacity before the patch */
} bt_capacity_req;

static int bt_kernel_patch(void *td, void *args);

/* Patches (or restores) the driver's allowlist. Returns 0 and sets *rc, or a
 * negative errno from the syscall itself. */
static int bt_kernel_run(int (*fn)(void *, void *), void *arg)
{
    return (int)syscall(SYS_KEXEC, fn, arg, 0, 0, 0, 0);
}

/* Where the scan keeps what it reads: the function runs in kernel mode, so it
 * keeps off the stack and calls nothing. */
static uint32_t g_scan_vid[12];
static uint32_t g_scan_pid[12];

/* The dword at `p` is the immediate of a `cmp r32, imm32`. */
static int bt_cmp_imm(const uint8_t *p)
{
    if (p[-1] == 0x3D) return 1;                                /* cmp eax, id */
    if (p[-2] == 0x81 && ((p[-1] >> 3) & 7) == 7) return 1;     /* cmp r/m32, id */
    if (p[-3] == 0x81 && ((p[-2] >> 3) & 7) == 7) return 1;
    return 0;
}

/* Reads the comparisons of 044F and 1000 and pairs them up: the Sony compiler
 * keeps the same distance between each vendor id and its product id on every
 * firmware (0x30, 0x33, 0xBF2, 0x4E1, 0x4F3), so the pair is that one, or the
 * nearest product id after it. The five of the list sit within 0x8000 bytes of
 * each other, while a stray 044F comparison sits about 85 KB earlier on 12.00
 * and later: the list is the first compact group of five. What it reads is
 * reported, not written: on 13.52 the fifth product id sits behind its vendor
 * id, which no rule can tell from a stray comparison. Returns the number of
 * pairs, or 0. */
static int bt_scan(uint64_t base, uint32_t off[BT_PAIRS][2])
{
    static const uint32_t delta[5] = { 0x30, 0x33, 0xBF2, 0x4E1, 0x4F3 };
    uint32_t a, end, n, i, j;
    uint8_t used[12];
    int nv = 0, np = 0;

    for (a = BT_SCAN_START; a + 8 <= BT_SCAN_END && nv < 12; a++)
        if (*(volatile uint32_t *)(base + a) == 0x044F && bt_cmp_imm((const uint8_t *)(base + a)))
            g_scan_vid[nv++] = a;

    for (i = 0; i + 4 < (uint32_t)nv; i++) {
        uint32_t span = g_scan_vid[i + 4] - g_scan_vid[i];
        uint32_t near = g_scan_vid[i + 1] - g_scan_vid[i];

        if (span <= 0x8000 && near >= 0x400 && near <= 0x1000) break;
    }
    if (i + 4 < (uint32_t)nv) n = 5;
    else if (nv >= 5) n = nv < BT_PAIRS ? (uint32_t)nv : (uint32_t)BT_PAIRS;
    else return 0;

    end = g_scan_vid[n - 1] + 0x8000;
    if (end > BT_SCAN_END) end = BT_SCAN_END;
    a = g_scan_vid[0] > 0x800 ? g_scan_vid[0] - 0x800 : 0;
    for (; a + 8 <= end && np < 12; a++)
        if (*(volatile uint32_t *)(base + a) == 0x1000 && bt_cmp_imm((const uint8_t *)(base + a)))
            g_scan_pid[np++] = a;
    if ((uint32_t)np < n) return 0;

    for (i = 0; i < 12; i++) used[i] = 0;
    for (i = 0; i < n; i++) {
        uint32_t best = 0, bestd = 0xFFFFFFFF;
        int found = 0;

        if (i < 5) {
            uint32_t want = g_scan_vid[i] + delta[i];

            for (j = 0; j < (uint32_t)np; j++)
                if (!used[j] && g_scan_pid[j] + 16 >= want && g_scan_pid[j] <= want + 16) {
                    best = j;
                    found = 1;
                    break;
                }
        }
        if (!found)
            for (j = 0; j < (uint32_t)np; j++) {
                uint32_t d;

                if (used[j] || g_scan_pid[j] <= g_scan_vid[i]) continue;
                d = g_scan_pid[j] - g_scan_vid[i];
                if (d < 4096 && d < bestd) { bestd = d; best = j; found = 1; }
            }
        if (!found)
            for (j = 0; j < (uint32_t)np; j++) {
                uint32_t d;

                if (used[j]) continue;
                d = g_scan_pid[j] > g_scan_vid[i] ? g_scan_pid[j] - g_scan_vid[i]
                                                  : g_scan_vid[i] - g_scan_pid[j];
                if (d < bestd) { bestd = d; best = j; found = 1; }
            }
        if (!found) return 0;
        used[best] = 1;
        off[i][0] = g_scan_vid[i];
        off[i][1] = g_scan_pid[best];
    }
    return (int)n;
}

/* The list patch. Runs in kernel mode: no calls, no libc, small stack. */
static int bt_kernel_patch(void *td, void *args)
{
    kexec_args *ka = (kexec_args *)args;
    bt_patch_req *r = (bt_patch_req *)ka->arg1;
    uint64_t base, flags, cr0;
    uint32_t lo, hi;
    int i, np = 0, original = 0, same = 0, patched = 0;

    (void)td;

    /* The kernel's base is the syscall entry point minus its prologue. */
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000082));
    base = ((uint64_t)hi << 32 | lo) - 0x1C0;
    if ((base >> 48) != 0xFFFF || (base & 0x3FFF)) { r->rc = 0xE1; return 22; }
    if (r->nids < 1 || r->nids > BT_PAIRS) { r->rc = 0xE4; return 22; }

    /* Where the list is on this firmware: a table, and nothing else is
     * written. */
    for (i = 0; i < (int)(sizeof k_offsets / sizeof k_offsets[0]); i++) {
        int k;

        if (k_offsets[i].fw != r->fw) continue;
        np = k_offsets[i].npairs;
        for (k = 0; k < np; k++) {
            r->off[k][0] = k_offsets[i].off[k][0];
            r->off[k][1] = k_offsets[i].off[k][1];
        }
        break;
    }
    if (!np) {
        /* No table: read the text and say what is there, so it can be added.
         * Before 9.00 the list is elsewhere and even that is not tried. */
        if (r->fw && r->fw < 0x0900) { r->rc = 0xE2; return 22; }
        r->npairs = bt_scan(base, r->off);
        r->dynamic = 1;
        r->rc = r->npairs ? 0xE6 : 0xE5;
        return 22;
    }
    r->npairs = np;

    /* Every word has to say something this payload could have written: the
     * original id, the requested ones, Sony's (OneFamily), or another of the
     * families'. Anything else means the offsets are not what we think. */
    for (i = 0; i < np; i++) {
        uint32_t v = *(volatile uint32_t *)(base + r->off[i][0]);
        uint32_t p = *(volatile uint32_t *)(base + r->off[i][1]);
        uint32_t tv = r->ids[i % r->nids][0], tp = r->ids[i % r->nids][1];
        int k, known = 0;

        if (v == 0x044F && p == 0x1000) { original++; continue; }
        if ((v == tv && p == tp) || (v == 0x054C && p == 0x0CE6)) { same++; continue; }
        for (k = 0; k < r->nknown && k < BT_KNOWN_PAIRS; k++)
            if (v == r->known[k][0] && p == r->known[k][1]) { known = 1; break; }
        if (known) { patched++; continue; }
        r->rc = 0x100 + (uint32_t)(2 * i);
        return 22;
    }

    /* Nothing to write: already the requested pair, or already restored. */
    if (!r->restore && same == np) { r->rc = 0x0100; return 0; }
    /* Half patched: another payload did it, or died doing it. Do not guess. */
    if (!r->restore && original && (same || patched)) {
        r->rc = (int)(0x8000 | same << 8 | original);
        return 22;
    }

    __asm__ volatile("pushfq; popq %0; cli; mov %%cr0, %1" : "=r"(flags), "=r"(cr0) :: "memory");
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0 & ~(1UL << 16)) : "memory");
    for (i = 0; i < np; i++) {
        *(volatile uint32_t *)(base + r->off[i][0]) = r->restore ? 0x044F : r->ids[i % r->nids][0];
        *(volatile uint32_t *)(base + r->off[i][1]) = r->restore ? 0x1000 : r->ids[i % r->nids][1];
    }
    __asm__ volatile("mov %0, %%cr0; pushq %1; popfq" :: "r"(cr0), "r"(flags) : "memory", "cc");

    r->rc = 0;
    return 0;
}

static int bt_kernel_capacity(void *td, void *args);

/* Raises (or gives back) one link's output report capacity. Runs in kernel
 * mode: the bthid context is a pointer in the kernel's data. */
/* One link's capacity: the pad's address at +0x30 of its entry, the device
 * (its vendor and product id) at +0x638, the output report size at +0x1574.
 * Returns 0, or a code saying why not. */
static int bt_capacity_one(uint8_t *ctx, bt_capacity_req *r)
{
    uint32_t o;

    if (!ctx || ((uint64_t)ctx >> 48) != 0xFFFF) return 0xE3;
    for (o = 0x42A8; o < 0x185A0; o += 0x1588) {
        uint8_t *link = ctx + o;
        uint8_t *dev;
        volatile uint32_t *capacity;

        if (*(volatile uint64_t *)(link + 0x30) != r->address) continue;

        dev = *(uint8_t **)(link + 0x638);
        if (!dev || ((uint64_t)dev >> 48) != 0xFFFF) return 0xE4;
        if (*(volatile uint16_t *)(dev + 0x28) != r->vid ||
            *(volatile uint16_t *)(dev + 0x2A) != r->pid) return 0xE5;

        capacity = (volatile uint32_t *)(link + 0x1574);
        if (*capacity != 22 && *capacity != 49 && *capacity != 78) return 0xE6;
        if (r->restore) {
            if (r->original != 22 && r->original != 49 && r->original != 78) return 0xE7;
            *capacity = r->original;
        } else {
            r->original = *capacity;
            *capacity = 78;
        }
        return 0;
    }
    return 0xE8;                       /* no link with that address yet */
}

/* Raises (or gives back) one link's output report capacity. Runs in kernel
 * mode: the bthid context is a pointer in the kernel's data. */
static int bt_kernel_capacity(void *td, void *args)
{
    kexec_args *ka = (kexec_args *)args;
    bt_capacity_req *r = (bt_capacity_req *)ka->arg1;
    uint64_t base;
    uint32_t lo, hi;

    (void)td;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000082));
    base = ((uint64_t)hi << 32 | lo) - 0x1C0;
    if ((base >> 48) != 0xFFFF || (base & 0x3FFF)) { r->rc = 0xE1; return 22; }
    if (!r->address || (r->address >> 48)) { r->rc = 0xE2; return 22; }

    /* The context is at a fixed place on 12.50, 13.02 and 13.52; on a firmware
     * where it is not, the pointers near it are tried (the window is kernel
     * data either way, and a wrong one has to look like a pointer). */
    r->rc = (int)bt_capacity_one(*(uint8_t **)(base + 0x2612550), r);
    if (r->rc) {
        uint32_t cand;

        for (cand = 0x2612000; cand <= 0x2613000; cand += 8) {
            if (cand == 0x2612550) continue;
            if (bt_capacity_one(*(uint8_t **)(base + cand), r) == 0) { r->rc = 0; break; }
        }
    }
    return r->rc ? 22 : 0;
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
    pad_output out_want;        /* what the console last asked (rumble, lights) */
    int out_have;               /* out_want has been filled at least once */
    uint8_t raw[BT_REPORT_MAX];
    int raw_len;
    pad_state st;
} bt_dev;

struct bt_pad_mgr_s {
    int fd;
    int kq;
    int patched;                /* we patched the driver's list */
    int inherited;              /* it was patched before we started */
    const bt_family *fam;       /* which pads were asked for */
    int poll_mode;              /* kevent was refused: read /dev/bt non-blocking */
    uint16_t text_vid, text_pid;    /* the id the driver's list holds now */
    int attempt;                /* how many ids have been offered */
    pad_output test;            /* a rumble asked for by hand, for a moment */
    long test_until;
    int test_on;
    int registered[BT_FAMILY_IDS];
    int nreg;
    long next_reg;              /* when to offer the refused ids again */
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
    snprintf(d->name, sizeof d->name, "%s", m->fam->label);
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

static int bt_capacity(bt_pad_mgr_t *m, bt_dev *d, int restore)
{
    bt_capacity_req req;

    memset(&req, 0, sizeof req);
    req.address = d->address;
    req.restore = restore ? 1 : 0;
    req.original = restore ? d->capacity_original : 0;
    req.vid = m->fam->ids[0][0];
    req.pid = m->fam->ids[0][1];

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
    int n;
    long rc;

    if (!d->connected || d->out_unsupported) return;
    if (!m->fam->build_output) return;

    n = m->fam->build_output(&d->out, out, buf, sizeof buf);
    if (n <= 0) return;

    /* The driver sizes the link for a 22-byte pad; a 78-byte report through
     * it makes the DualSense drop the link. A shorter one (the Switch Pro's
     * 10 bytes) fits as it is. */
    if (m->fam->needs_capacity && !d->capacity) {
        if (bt_capacity(m, d, 0) != 0) {
            d->out_unsupported = 1;
            log_line("bt_pad: %012llx: no output until it reconnects (capacity)", (unsigned long long)d->address);
            return;
        }
        d->capacity = 1;
    }

    memset(&req, 0, sizeof req);
    req.address = d->address;
    req.buf = buf;
    req.len = n;
    req.report = buf[0];
    rc = syscall(SYS_ioctl, m->fd, IOCTL_BT_SEND_REPORT, &req, 0, 0, 0);
    if (rc == 0) {
        d->out_errors = 0;
        d->out_started = 1;
        d->out_want = *out;
        d->out_have = 1;
        d->last_out = now_ms();
    } else if ((uint32_t)rc != 0x802F3707U) {
        if (++d->out_errors >= 3) {
            d->out_unsupported = 1;
            log_line("bt_pad: %012llx: output reports refused (0x%lx), off", (unsigned long long)d->address, (unsigned long)rc);
        }
    }
}

/* Buzzes and lights the pads for a moment, to check the output path without a
 * game (the menu's test button). The values are absolute, not relative to what
 * the console asked. */
int bt_pad_test(bt_pad_mgr_t *m, const pad_output *out, long ms)
{
    int i, live = 0;

    if (!m || !out) return 0;
    m->test = *out;
    if (ms <= 0) ms = 500;
    if (ms > 5000) ms = 5000;
    m->test_until = now_ms() + ms;
    m->test_on = 1;
    for (i = 0; i < BT_MAX_DEVS; i++)
        if (m->devs[i].address && m->devs[i].connected) live = 1;
    return live;
}

const char *bt_pad_family(const bt_pad_mgr_t *m)
{
    return m && m->fam ? m->fam->name : "";
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
    if (!m->fam->accept(rep, len) || m->fam->parse(rep, len, &st) == PARSE_NONE) return;

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
        if (!len) continue;
        if (!m->fam->accept(rep, len)) {
            /* A report this family does not send. Show a few, so a family for
             * another pad can be written from what the console really gets. */
            static int shown;

            if (shown < 12) {
                char hex[3 * 24 + 1];
                int j, used = 0;

                shown++;
                for (j = 0; j < len && j < 24; j++)
                    used += snprintf(hex + used, sizeof hex - (size_t)used, "%02X ", rep[j]);
                log_line("bt_pad: %012llx: not %s (%d bytes): %s", (unsigned long long)address,
                         m->fam->name, len, hex);
            }
            continue;
        }

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
        pad_output o;
        long every;

        if (!d->address || !d->connected) continue;

        /* The test's time is up: the neutral report goes out at once, not at
         * the next heartbeat, so the pad does not keep buzzing for 2.5 s. */
        if (m->test_on && now >= m->test_until) {
            m->test_on = 0;
            d->last_out = 0;
        }

        every = d->out_started ? BT_OUT_EVERY_MS : BT_OUT_FIRST_MS;
        if (now < m->test_until) every = BT_TEST_MS;    /* a rumble asked for by hand */
        if (d->last_out && now - d->last_out < every) continue;

        /* The console's last ask, so a long rumble is not cut short by the
         * heartbeat; before any, the PlayStation blue the menu shows. */
        o = d->out_have ? d->out_want : neutral;
        if (now < m->test_until) o = m->test;
        if (!o.r && !o.g && !o.b) {
            o.r = neutral.r;
            o.g = neutral.g;
            o.b = neutral.b;
        }
        o.player = (uint8_t)(MAX_PADS - d->slot);
        dev_output(m, d, &o);
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
        /* Every 16 ms, report or not: some pads (the Switch Pro) send a report
         * only when something changes, and the console wants a steady stream
         * to keep the virtual pad's state. */
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
    if (m->poll_mode) return 1;         /* the fd is non-blocking: read() says */
    {
        struct bt_kevent event;
        struct bt_timespec timeout;

        memset(&event, 0, sizeof event);
        memset(&timeout, 0, sizeof timeout);        /* do not wait */
        return syscall(SYS_kevent, m->kq, 0, 0, &event, 1, &timeout) > 0;
    }
}

static int bt_register(bt_pad_mgr_t *m, uint16_t vid, uint16_t pid);
static void bt_patch_fill(bt_pad_mgr_t *m, bt_patch_req *req, uint16_t vid, uint16_t pid);
static int bt_patch_id(bt_pad_mgr_t *m, uint16_t vid, uint16_t pid);

/* The driver only registers an id that has a live link behind it, so a pad
 * turned on after the module started is refused at first (ENOENT). The
 * refuse is not final: the ids are offered again every few seconds, and that
 * is what lets a pad connected later be taken. */
#define BT_REG_RETRY_MS 3000

static void dev_register_retry(bt_pad_mgr_t *m, long now)
{
    int i;

    if (m->nreg >= m->fam->nids) return;
    if (m->next_reg && now < m->next_reg) return;
    m->next_reg = now + BT_REG_RETRY_MS;

    /* One id at a time: the list has to hold the id of the pad that will
     * connect, so the family's first id keeps it almost always and the others
     * get every fourth try (a pad whose channel was refused does not try
     * again, so spending half the time on the wrong id costs a reconnect). */
    i = (m->fam->nids > 1 && ++m->attempt % 4 == 0) ? 1 : 0;
    if (i >= m->fam->nids || m->registered[i]) {
        for (i = 0; i < m->fam->nids && m->registered[i]; i++) { }
        if (i >= m->fam->nids) return;
    }

    {
        uint16_t vid = m->fam->ids[i][0], pid = m->fam->ids[i][1];

        if (bt_patch_id(m, vid, pid) != 0) return;
        if (!bt_register(m, vid, pid)) {
            /* Leave the list on the id that works, not on the one just tried. */
            if (m->nreg > 0) bt_patch_id(m, m->fam->ids[0][0], m->fam->ids[0][1]);
            return;
        }
        m->registered[i] = 1;
        m->nreg++;
        log_line("bt_pad: registered %04X:%04X", vid, pid);
    }
}

void bt_pad_poll(bt_pad_mgr_t *m, long now)
{
    uint8_t events[256];
    long n, i;

    if (!m) return;

    dev_register_retry(m, now);
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

/* Puts one id in the driver's list, all six comparisons, once. */
static int bt_patch_id(bt_pad_mgr_t *m, uint16_t vid, uint16_t pid)
{
    bt_patch_req req;
    int i;

    if (m->text_vid == vid && m->text_pid == pid) return 0;

    bt_patch_fill(m, &req, vid, pid);
    if (bt_kernel_run(bt_kernel_patch, &req) != 0) {
        log_line("bt_pad: allowlist patch: the kernel call failed");
        return -1;
    }
    if (req.rc == 0xE6) {
        /* No table for this firmware: what the text says, for a table. */
        log_line("bt_pad: firmware 0x%04X has no table for the driver's list", m->fw);
        for (i = 0; i < req.npairs; i++)
            log_line("bt_pad: pair %d at %06X/%06X", i, req.off[i][0], req.off[i][1]);
        log_line("bt_pad: send those lines along and the firmware gets one");
        return -1;
    }
    if (req.rc & 0x8000) {
        log_line("bt_pad: the allowlist is half patched by another payload (0x%x); left alone", req.rc);
        return -1;
    }
    if (req.rc && req.rc != 0x0100) {
        log_line("bt_pad: allowlist patch failed: rc 0x%x", req.rc);
        return -1;
    }
    if (req.rc & 0xFF00) {
        m->inherited = 1;
    } else {
        m->patched = 1;
        log_line("bt_pad: allowlist set to %04X:%04X (%d pairs)", vid, pid, req.npairs);
    }
    m->text_vid = vid;
    m->text_pid = pid;
    return 0;
}

/* What the kernel side needs to know: the family's ids, and every id this
 * build's families know, so a list patched by a previous run (or by OneFamily)
 * is told from one that was never touched. */
/* The six comparisons are alternatives, not slots: each one has to accept the
 * same pad, or the connect falls through to a check that still wants an old id.
 * Cycling the family's ids through them is what kept the DualSense out after
 * the Edge was added: half of the list then only took 054C:0DF2. One id, in
 * all six, and the list is moved to the other id when that one is tried. */
static void bt_patch_fill(bt_pad_mgr_t *m, bt_patch_req *req, uint16_t vid, uint16_t pid)
{
    int i;

    memset(req, 0, sizeof *req);
    req->fw = m->fw;
    req->nids = 1;
    for (i = 0; i < BT_PAIRS; i++) {
        req->ids[i][0] = vid;
        req->ids[i][1] = pid;
    }
    req->nknown = bt_family_known_ids(req->known, BT_KNOWN_PAIRS);
}

bt_pad_mgr_t *bt_pad_init(const bt_family *family,
                          bt_pad_feed_fn on_feed,
                          bt_pad_attach_fn on_attach,
                          bt_pad_detach_fn on_detach)
{
    bt_pad_mgr_t *m;
    uint64_t mask = 0x1FF000000ULL;
    int i;

    m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->fd = m->kq = -1;
    m->fam = family ? family : bt_family_default();
    m->on_feed = on_feed;
    m->on_attach = on_attach;
    m->on_detach = on_detach;
    for (i = 0; i < BT_MAX_DEVS; i++) m->devs[i].slot = -1;

    m->fw = fw_bcd();
    /* Before 9.00 the driver's list is elsewhere and none of this was tried. */
    if (m->fw && m->fw < 0x0900) {
        log_line("bt_pad: firmware 0x%04X not supported (9.00 and later)", m->fw);
        free(m);
        return NULL;
    }
    if (m->fw != 0x1352)
        log_line("bt_pad: firmware 0x%04X: not the one this was tried on (13.52)", m->fw);

    m->fd = open("/dev/bt", O_RDWR);
    if (m->fd < 0) {
        log_line("bt_pad: open /dev/bt: %s", strerror(errno));
        free(m);
        return NULL;
    }

    m->kq = (int)syscall(SYS_kqueue, 0, 0, 0);
    if (m->kq < 0) {
        log_line("bt_pad: kqueue(): %s", strerror(errno));
        close(m->fd);
        free(m);
        return NULL;
    }
    /* EVFILT_READ on /dev/bt is not always accepted (the console's driver
     * says ENOENT for it after a badge state it does not like). The events are
     * plain reads, so reading the fd non-blocking gets them anyway. */
    if (!bt_watch(m)) {
        log_line("bt_pad: kevent(add /dev/bt) refused (%s); reading it directly", strerror(errno));
        close(m->kq);
        m->kq = -1;
        m->poll_mode = 1;
        fcntl(m->fd, F_SETFL, fcntl(m->fd, F_GETFL) | O_NONBLOCK);
    }

    /* Everything the driver reports about controllers, from now on. */
    if (write(m->fd, &mask, sizeof mask) != (long)sizeof mask) {
        log_line("bt_pad: subscribe to /dev/bt: %s", strerror(errno));
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }

    if (bt_patch_id(m, m->fam->ids[0][0], m->fam->ids[0][1]) != 0) {
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }

    /* Every id of the family is offered to the driver; one id per model, and
     * only the connected one can be registered (the rest answer ENOENT). */
    /* The driver only takes an id that has a live channel behind it, and one
     * pad is what this is for: the first id that answers is the one kept. */
    for (i = 0; i < m->fam->nids && !m->nreg; i++) {
        uint16_t vid = m->fam->ids[i][0], pid = m->fam->ids[i][1];

        if (bt_patch_id(m, vid, pid) != 0) break;
        if (bt_register(m, vid, pid)) {
            m->registered[i] = 1;
            m->nreg++;
            break;
        }
        log_line("bt_pad: register %04X:%04X refused: %s", vid, pid, strerror(errno));
    }
    if (!m->nreg && bt_patch_id(m, m->fam->ids[0][0], m->fam->ids[0][1]) != 0) {
        close(m->kq);
        close(m->fd);
        free(m);
        return NULL;
    }
    /* No pad of the family connected yet is not a failure: the patch and the
     * subscriptions are in place, and dev_register_retry() keeps offering the
     * ids, so a pad turned on later is taken over (asking the user to reload
     * with the pad already on was a needless trap). */
    if (m->nreg == 0)
        log_line("bt_pad: no %s connected yet; its ids are offered while this runs", m->fam->label);

    log_line("bt_pad: ready (%s, firmware 0x%04X, up to %d pads)", m->fam->name, m->fw, BT_MAX_DEVS);
    return m;
}

void bt_pad_close(bt_pad_mgr_t *m)
{
    int i;

    if (!m) return;

    /* The pads are still connected: give their links back first. */
    for (i = 0; i < BT_MAX_DEVS; i++)
        if (m->devs[i].address && m->devs[i].capacity)
            bt_capacity(m, &m->devs[i], 1);

    for (i = 0; i < BT_MAX_DEVS; i++)
        if (m->devs[i].live && m->on_detach) m->on_detach(m->devs[i].slot);

    for (i = m->fam->nids - 1; i >= 0; i--)
        if (m->registered[i])
            bt_unregister(m, m->fam->ids[i][0], m->fam->ids[i][1]);

    if (m->patched) {
        bt_patch_req req;

        bt_patch_fill(m, &req, m->fam->ids[0][0], m->fam->ids[0][1]);
        req.restore = 1;
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
