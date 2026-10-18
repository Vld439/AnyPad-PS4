/* Controllers on the console's own USB ports.
 *
 * The device's descriptors say which interface is a controller and how it
 * talks:
 *   Xbox 360 (XInput)  class ff/5d/01  reports 00 14 ..., sent unasked
 *   Xbox One (GIP)     class ff/47/d0  silent until told to power on
 *   HID                class 03        a profile by vid:pid, or the generic
 *                                      one if the report descriptor shows a
 *                                      gamepad (a keyboard interface does not)
 * A system driver holding the interface is detached first: that is what
 * makes the console see a GIP pad as a headset or a pad as a keyboard.
 *
 * The PS4 has no ugen nodes: the pads are reached through libSceUsbd (libusb
 * under another name), read by one thread per pad whose reports the main
 * loop parses.
 *
 * The virtual pad is only asked for after the first report has been read,
 * so a controller that never speaks does not leave the console asking who
 * is using a pad that cannot answer.
 *
 * USB pads take the virtual pad slots from the top (3, 2, ...), network
 * clients from the bottom. */
#include "usb_pad.h"
#include "log.h"
#include "profiles.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#define MAX_USB_DEVS 4
#define MAX_SEEN 16
#define USB_SCAN_INTERVAL_MS 2000
#define WAKE_TRIES 5
#define WAKE_EVERY_MS 1000
#define RX_LOG_MAX 25
#define TRIGGER_PRESS 0x20
#define SETTLE_MS 1500               /* the console's own events after the driver is taken */
#define MAP_DIR "/data/anypad/maps"
#define REPORT_MAX 64
#define QUEUE_LEN 16

enum { KIND_NONE, KIND_X360, KIND_GIP, KIND_HID };
static const char *const k_kind[] = { "?", "Xbox 360", "Xbox One (GIP)", "HID" };

typedef struct {
    int iface, kind;
    uint8_t ep_in, ep_out;
} usb_iface;

typedef struct usbd_dev usbd_dev;
typedef struct usbd_handle usbd_handle;

typedef struct {
    char path[32];                  /* usb:bus.address */
    uint16_t vid, pid;
    int kind, iface, uhid;
    const pad_profile *profile;
    union { unsigned char bytes[PROFILE_CTX_MAX]; uint64_t align; } pctx;
    pad_state st;
    int slot, active, announced, got_input;
    long opened_at;
    uint8_t ep_in, ep_out;
    uint8_t gip_seq;
    int wake_tries;
    long next_wake;
    int hs_step, init_step, init_rounds, init_done, full;   /* HID profiles' wake-up */
    int rx_logged, errors;
    usbd_handle *h;
    pthread_t thread;
    int thread_on;
    volatile int quit, gone;
    pthread_mutex_t lock;
    uint8_t q[QUEUE_LEN][REPORT_MAX];
    int qlen[QUEUE_LEN];
    int qhead, qcount;
} usb_dev_t;

struct usb_pad_mgr_s {
    usb_dev_t devs[MAX_USB_DEVS];
    char seen[MAX_SEEN][32];        /* devices looked at and left alone */
    usb_pad_feed_fn on_feed;
    usb_pad_attach_fn on_attach;
    usb_pad_detach_fn on_detach;
    long last_scan;
};

static int out_send(usb_dev_t *d, const uint8_t *p, int len);
static int hid_report_desc(usb_dev_t *d, int iface, uint8_t *buf, int max);

static uint8_t stick16(int16_t v) { return (uint8_t)(((int)v + 32768) >> 8); }
static uint8_t stick16_inv(int16_t v) { return (uint8_t)((32767 - (int)v) >> 8); }

static uint32_t trigger_bits(uint8_t l2, uint8_t r2)
{
    return (l2 > TRIGGER_PRESS ? PAD_L2 : 0) | (r2 > TRIGGER_PRESS ? PAD_R2 : 0);
}

/* ---- descriptors ---------------------------------------------------------- */

/* The interfaces that may be a controller, alternate setting 0, with their
 * interrupt endpoints. */
static int read_ifaces(const uint8_t *cfg, int len, usb_iface *out, int max)
{
    int off = 0, n = 0, cur = -1;

    while (off + 2 <= len) {
        const uint8_t *d = cfg + off;
        int dl = d[0];

        if (dl < 2 || off + dl > len) break;
        if (d[1] == 0x04 && dl >= 9) {
            int kind = KIND_NONE;

            cur = -1;
            if (d[3] == 0) {
                if (d[5] == 0xFF && d[6] == 0x5D && d[7] == 0x01) kind = KIND_X360;
                else if (d[5] == 0xFF && d[6] == 0x47 && d[7] == 0xD0) kind = KIND_GIP;
                else if (d[5] == 0x03) kind = KIND_HID;
            }
            if (kind != KIND_NONE && n < max) {
                memset(&out[n], 0, sizeof out[n]);
                out[n].iface = d[2];
                out[n].kind = kind;
                cur = n++;
            }
        } else if (d[1] == 0x05 && dl >= 7 && cur >= 0 && (d[3] & 3) == 3) {
            if (d[2] & 0x80) {
                if (!out[cur].ep_in) out[cur].ep_in = d[2];
            } else if (!out[cur].ep_out) {
                out[cur].ep_out = d[2];
            }
        }
        off += dl;
    }
    return n;
}

/* The interfaces and their classes, for the log. */
static void log_ifaces(const char *path, const uint8_t *cfg, int len)
{
    char line[200];
    int off = 0, used;

    used = snprintf(line, sizeof line, "usb_pad: %s interfaces:", path);
    while (off + 2 <= len && used < (int)sizeof line - 24) {
        const uint8_t *d = cfg + off;

        if (d[0] < 2 || off + d[0] > len) break;
        if (d[1] == 0x04 && d[0] >= 9)
            used += snprintf(line + used, sizeof line - (size_t)used, " %d.%d=%02x/%02x/%02x",
                             d[2], d[3], d[5], d[6], d[7]);
        off += d[0];
    }
    log_line("%s", line);
}

/* Picks the controller interface: XInput or GIP first, then HID with a
 * profile, then HID whose report descriptor is a gamepad. */
static int choose_iface(usb_dev_t *d, const usb_iface *ifs, int nifs)
{
    static uint8_t desc[1024];
    int pass, i;

    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < nifs; i++) {
            const usb_iface *f = &ifs[i];
            int len;

            if (!f->ep_in) continue;
            if (pass == 0 && f->kind != KIND_X360 && f->kind != KIND_GIP) continue;
            if (pass == 1 && f->kind != KIND_HID) continue;
            if (f->kind == KIND_HID) {
                d->profile = profile_find(d->vid, d->pid);
                if (d->profile) {
                    if (d->profile->setup) d->profile->setup(d->pctx.bytes, d->vid, d->pid);
                } else {
                    len = hid_report_desc(d, f->iface, desc, sizeof desc);
                    if (len <= 0) {
                        log_line("usb_pad: %s: interface %d: no report descriptor (%d)", d->path, f->iface, len);
                        continue;
                    }
                    if (!generic_setup(d->pctx.bytes, desc, len, d->vid, d->pid, MAP_DIR)) {
                        log_line("usb_pad: %s: interface %d is HID but not a gamepad", d->path, f->iface);
                        continue;
                    }
                    d->profile = &generic_profile;
                }
            }
            d->kind = f->kind;
            d->iface = f->iface;
            d->ep_in = f->ep_in;
            d->ep_out = f->ep_out;
            return 1;
        }
    return 0;
}

/* ---- wake-up -------------------------------------------------------------- */

/* A Nintendo pad on USB first wants its own handshake (as hid-nintendo
 * does: handshake, 3 Mbit, handshake, no USB timeout), or it drops off the
 * bus within a second; then the profile's own wake-up. */
static const uint8_t k_switch_usb[4][2] = { { 0x80, 0x02 }, { 0x80, 0x03 }, { 0x80, 0x02 }, { 0x80, 0x04 } };

static void hid_wake(usb_dev_t *d, long now)
{
    uint8_t buf[REPORT_MAX];
    pad_output o;
    int n;

    if (d->vid == 0x057E && d->hs_step < 4) {
        memset(buf, 0, sizeof buf);
        memcpy(buf, k_switch_usb[d->hs_step], 2);
        out_send(d, buf, sizeof buf);
        d->hs_step++;
        d->next_wake = now + 100;
        return;
    }
    /* A Switch keeps sending rumble to its pads; Switch-mode clones (the
     * GameSir G8 Plus) drop off the bus after 10-25 s without it. */
    if (d->vid == 0x057E && d->init_done) {
        static const uint8_t idle[8] = { 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };

        memset(buf, 0, sizeof buf);
        buf[0] = 0x10;
        buf[1] = (uint8_t)(d->gip_seq++ & 0x0F);
        memcpy(buf + 2, idle, sizeof idle);
        out_send(d, buf, sizeof buf);
        d->next_wake = now + 500;
        return;
    }
    if (!d->profile->init_report || d->init_done) return;
    memset(&o, 0, sizeof o);
    o.player = (uint8_t)(MAX_PADS - d->slot);
    memset(buf, 0, sizeof buf);
    n = d->profile->init_report(d->pctx.bytes, d->init_step, &o, buf, sizeof buf);
    if (n > 0) {
        out_send(d, buf, d->vid == 0x057E ? (int)sizeof buf : n);
        d->init_step++;
        d->next_wake = now + 50;
        return;
    }
    d->init_rounds++;
    if (!d->profile->needs_full || d->full || d->init_rounds >= WAKE_TRIES) {
        d->init_done = 1;
        log_line("usb_pad: %s: wake-up done (%d round(s), full report %s)", d->path, d->init_rounds,
                 d->full ? "seen" : "not seen");
        return;
    }
    d->init_step = 0;
    d->next_wake = now + WAKE_EVERY_MS;
}

/* GIP pads send nothing until powered on; a 360 pad only gets its player
 * light; HID profiles get their wake-up sequence. */
static void wake(usb_dev_t *d, long now)
{
    if (now < d->next_wake) return;
    if (d->kind == KIND_HID && d->profile) {
        hid_wake(d, now);
        return;
    }
    if (d->got_input || d->wake_tries >= WAKE_TRIES) return;
    if (d->kind == KIND_GIP) {
        uint8_t on[5] = { 0x05, 0x20, 0x00, 0x01, 0x00 };

        on[2] = d->gip_seq++;
        if (!out_send(d, on, sizeof on)) return;
        log_line("usb_pad: %s: power-on sent (%d)", d->path, d->wake_tries + 1);
    } else if (d->kind == KIND_X360) {
        uint8_t led[3] = { 0x01, 0x03, 0x00 };

        led[2] = (uint8_t)(0x06 + d->slot % 4);
        out_send(d, led, sizeof led);
        d->wake_tries = WAKE_TRIES;
        return;
    } else {
        return;
    }
    d->wake_tries++;
    d->next_wake = now + WAKE_EVERY_MS;
}

/* ---- reports -------------------------------------------------------------- */

static int parse_x360(const uint8_t *r, int n, pad_state *st)
{
    uint32_t b = 0;

    if (n < 14 || r[0] != 0x00 || r[1] < 0x14) return PARSE_NONE;
    if (r[2] & 0x01) b |= PAD_UP;
    if (r[2] & 0x02) b |= PAD_DOWN;
    if (r[2] & 0x04) b |= PAD_LEFT;
    if (r[2] & 0x08) b |= PAD_RIGHT;
    if (r[2] & 0x10) b |= PAD_OPTIONS;      /* Start */
    if (r[2] & 0x20) b |= PAD_TOUCHPAD;     /* Back */
    if (r[2] & 0x40) b |= PAD_L3;
    if (r[2] & 0x80) b |= PAD_R3;
    if (r[3] & 0x01) b |= PAD_L1;
    if (r[3] & 0x02) b |= PAD_R1;
    if (r[3] & 0x04) b |= PAD_PS;           /* Guide */
    if (r[3] & 0x10) b |= PAD_CROSS;
    if (r[3] & 0x20) b |= PAD_CIRCLE;
    if (r[3] & 0x40) b |= PAD_SQUARE;
    if (r[3] & 0x80) b |= PAD_TRIANGLE;
    st->l2 = r[4];
    st->r2 = r[5];
    st->lx = stick16(les16(r + 6));
    st->ly = stick16_inv(les16(r + 8));
    st->rx = stick16(les16(r + 10));
    st->ry = stick16_inv(les16(r + 12));
    st->buttons = b | trigger_bits(st->l2, st->r2);
    return PARSE_OK;
}

/* GIP: 0x20 is the input report, 0x07 the Xbox button, which wants an
 * acknowledgement when the header asks for one. */
static int parse_gip(usb_dev_t *d, const uint8_t *r, int n, pad_state *st)
{
    uint32_t b = 0;

    if (r[0] == 0x07 && n >= 5) {
        if ((r[1] & 0x30) == 0x30) {
            uint8_t ack[13] = { 0x01, 0x20, 0x00, 0x09, 0x00, 0x07, 0x20, 0x02 };

            ack[2] = r[2];
            out_send(d, ack, sizeof ack);
        }
        st->buttons = (st->buttons & ~(uint32_t)PAD_PS) | (r[4] & 1 ? PAD_PS : 0);
        return PARSE_OK;
    }
    if (r[0] != 0x20 || n < 18) return PARSE_NONE;
    if (r[4] & 0x04) b |= PAD_OPTIONS;      /* Menu */
    if (r[4] & 0x08) b |= PAD_TOUCHPAD;     /* View */
    if (r[4] & 0x10) b |= PAD_CROSS;
    if (r[4] & 0x20) b |= PAD_CIRCLE;
    if (r[4] & 0x40) b |= PAD_SQUARE;
    if (r[4] & 0x80) b |= PAD_TRIANGLE;
    if (r[5] & 0x01) b |= PAD_UP;
    if (r[5] & 0x02) b |= PAD_DOWN;
    if (r[5] & 0x04) b |= PAD_LEFT;
    if (r[5] & 0x08) b |= PAD_RIGHT;
    if (r[5] & 0x10) b |= PAD_L1;
    if (r[5] & 0x20) b |= PAD_R1;
    if (r[5] & 0x40) b |= PAD_L3;
    if (r[5] & 0x80) b |= PAD_R3;
    st->l2 = (uint8_t)((le16(r + 6) & 0x3FF) >> 2);
    st->r2 = (uint8_t)((le16(r + 8) & 0x3FF) >> 2);
    st->lx = stick16(les16(r + 10));
    st->ly = stick16_inv(les16(r + 12));
    st->rx = stick16(les16(r + 14));
    st->ry = stick16_inv(les16(r + 16));
    st->buttons = b | (st->buttons & PAD_PS) | trigger_bits(st->l2, st->r2);
    return PARSE_OK;
}

static void handle(usb_pad_mgr_t *mgr, usb_dev_t *d, const uint8_t *r, int n)
{
    pad_state st = d->st;
    int res = PARSE_NONE;

    if (d->rx_logged < RX_LOG_MAX) {
        char hex[3 * 32 + 1];
        int i, k = n < 32 ? n : 32;

        for (i = 0; i < k; i++) snprintf(hex + 3 * i, 4, "%02x ", r[i]);
        hex[3 * k] = '\0';
        log_line("usb_pad: %s rx[%d]: %s", d->path, n, hex);
        d->rx_logged++;
    }

    if (d->kind == KIND_X360) res = parse_x360(r, n, &st);
    else if (d->kind == KIND_GIP) res = parse_gip(d, r, n, &st);
    else if (d->profile) res = d->profile->parse(d->pctx.bytes, r, n, &st);
    if (res == PARSE_NONE) return;
    if (res == PARSE_FULL) d->full = 1;

    d->got_input = 1;
    d->st = st;
    if (!d->announced && now_ms() - d->opened_at < SETTLE_MS) return;
    if (!d->announced) {
        d->announced = 1;
        log_line("usb_pad: %s speaks, virtual pad on slot %d", d->path, d->slot);
        if (mgr->on_attach) mgr->on_attach(d->slot, d->profile ? d->profile->name : k_kind[d->kind]);
    }
    if (mgr->on_feed) mgr->on_feed(d->slot, &d->st);
}

/* ---- the device table ----------------------------------------------------- */

static int seen(usb_pad_mgr_t *mgr, const char *path)
{
    int i;

    for (i = 0; i < MAX_SEEN; i++)
        if (strcmp(mgr->seen[i], path) == 0) return 1;
    return 0;
}

static void mark_seen(usb_pad_mgr_t *mgr, const char *path)
{
    int i;

    for (i = 0; i < MAX_SEEN; i++)
        if (!mgr->seen[i][0]) {
            snprintf(mgr->seen[i], sizeof mgr->seen[i], "%s", path);
            return;
        }
}

/* A free entry for `path`, or NULL if it is already open or there is no room. */
static usb_dev_t *new_dev(usb_pad_mgr_t *mgr, const char *path)
{
    usb_dev_t *d = NULL;
    int i;

    if (seen(mgr, path)) return NULL;
    for (i = 0; i < MAX_USB_DEVS; i++)
        if (mgr->devs[i].active && strcmp(mgr->devs[i].path, path) == 0) return NULL;
    for (i = 0; i < MAX_USB_DEVS && !d; i++)
        if (!mgr->devs[i].active) d = &mgr->devs[i];
    if (!d) return NULL;

    memset(d, 0, sizeof *d);
    snprintf(d->path, sizeof d->path, "%s", path);
    d->opened_at = now_ms();
    d->slot = MAX_PADS - 1 - (int)(d - mgr->devs);
    pad_state_reset(&d->st);
    return d;
}

static void log_taken(const usb_dev_t *d)
{
    log_line("usb_pad: %s (%04x:%04x): %s, interface %d, in %#x out %#x, profile %s, slot %d",
             d->path, d->vid, d->pid, k_kind[d->kind], d->iface, d->ep_in, d->ep_out,
             d->profile ? d->profile->name : "-", d->slot);
}

static void dev_close(usb_dev_t *d);

static void drop(usb_pad_mgr_t *mgr, usb_dev_t *d, const char *why)
{
    log_line("usb_pad: %s %s", d->path, why);
    dev_close(d);
    d->active = 0;
    if (d->announced && mgr->on_detach) mgr->on_detach(d->slot);
    d->announced = 0;
}

/* ---- PS4: libSceUsbd ------------------------------------------------------ */

#define USBD_ERR(r, code) ((r) == -(code) || \
                           ((((uint32_t)(r) & 0xFFFFFF00u) == 0x80240000u) && ((r) & 0xFF) == (code)))
#define USBD_NO_DEVICE 4
#define USBD_TIMEOUT   7
#define READ_TIMEOUT_MS 100

static struct {
    int ok;
    int (*init)(void);
    long (*get_list)(usbd_dev ***list);
    void (*free_list)(usbd_dev **list, int unref);
    int (*get_desc)(usbd_dev *dev, void *desc);
    uint8_t (*bus)(usbd_dev *dev);
    uint8_t (*addr)(usbd_dev *dev);
    int (*open)(usbd_dev *dev, usbd_handle **h);
    void (*close)(usbd_handle *h);
    int (*driver_active)(usbd_handle *h, int iface);
    int (*detach)(usbd_handle *h, int iface);
    int (*claim)(usbd_handle *h, int iface);
    int (*release)(usbd_handle *h, int iface);
    int (*intr)(usbd_handle *h, unsigned char ep, unsigned char *data, int len, int *done, unsigned int timeout);
    int (*ctrl)(usbd_handle *h, uint8_t type, uint8_t req, uint16_t value, uint16_t index,
                unsigned char *data, uint16_t len, unsigned int timeout);
} U;

/* Imported, so that the payload loader resolves them before main() and the
 * fork: in the forked process the system's module table is not ours. */
int sceUsbdInit(void);
long sceUsbdGetDeviceList(usbd_dev ***list);
void sceUsbdFreeDeviceList(usbd_dev **list, int unref);
int sceUsbdGetDeviceDescriptor(usbd_dev *dev, void *desc);
uint8_t sceUsbdGetBusNumber(usbd_dev *dev);
uint8_t sceUsbdGetDeviceAddress(usbd_dev *dev);
int sceUsbdOpen(usbd_dev *dev, usbd_handle **h);
void sceUsbdClose(usbd_handle *h);
int sceUsbdKernelDriverActive(usbd_handle *h, int iface);
int sceUsbdDetachKernelDriver(usbd_handle *h, int iface);
int sceUsbdClaimInterface(usbd_handle *h, int iface);
int sceUsbdReleaseInterface(usbd_handle *h, int iface);
int sceUsbdInterruptTransfer(usbd_handle *h, unsigned char ep, unsigned char *data, int len, int *done,
                             unsigned int timeout);
int sceUsbdControlTransfer(usbd_handle *h, uint8_t type, uint8_t req, uint16_t value, uint16_t index,
                           unsigned char *data, uint16_t len, unsigned int timeout);

static int usbd_load(void)
{
    int r;

    U.init = sceUsbdInit;
    U.get_list = sceUsbdGetDeviceList;
    U.free_list = sceUsbdFreeDeviceList;
    U.get_desc = sceUsbdGetDeviceDescriptor;
    U.bus = sceUsbdGetBusNumber;
    U.addr = sceUsbdGetDeviceAddress;
    U.open = sceUsbdOpen;
    U.close = sceUsbdClose;
    U.driver_active = sceUsbdKernelDriverActive;
    U.detach = sceUsbdDetachKernelDriver;
    U.claim = sceUsbdClaimInterface;
    U.release = sceUsbdReleaseInterface;
    U.intr = sceUsbdInterruptTransfer;
    U.ctrl = sceUsbdControlTransfer;
    r = U.init();
    log_line("usb_pad: sceUsbdInit %#x", (unsigned)r);
    if (r < 0) return 0;
    U.ok = 1;
    return 1;
}

static int out_send(usb_dev_t *d, const uint8_t *p, int len)
{
    uint8_t buf[REPORT_MAX];
    int done = 0, r;

    if (!d->ep_out || len <= 0 || len > (int)sizeof buf) return 0;
    memcpy(buf, p, (size_t)len);
    r = U.intr(d->h, d->ep_out, buf, len, &done, 200);
    if (r < 0 && d->errors++ < 5) log_line("usb_pad: %s: send %#x", d->path, (unsigned)r);
    return r >= 0;
}

static int hid_report_desc(usb_dev_t *d, int iface, uint8_t *buf, int max)
{
    return U.ctrl(d->h, 0x81, 0x06, 0x2200, (uint16_t)iface, buf, (uint16_t)max, 500);
}

/* One per pad: reads (and powers on a GIP pad) and leaves the reports for
 * the main loop. */
static void *reader(void *arg)
{
    usb_dev_t *d = arg;
    uint8_t buf[REPORT_MAX];
    int fails = 0;

    while (!d->quit) {
        int done = 0, r;

        wake(d, now_ms());
        r = U.intr(d->h, d->ep_in, buf, sizeof buf, &done, READ_TIMEOUT_MS);
        if (done > 0) {
            pthread_mutex_lock(&d->lock);
            if (d->qcount < QUEUE_LEN) {
                int k = (d->qhead + d->qcount) % QUEUE_LEN;

                memcpy(d->q[k], buf, (size_t)done);
                d->qlen[k] = done;
                d->qcount++;
            }
            pthread_mutex_unlock(&d->lock);
            fails = 0;
            continue;
        }
        if (r >= 0 || USBD_ERR(r, USBD_TIMEOUT)) continue;
        if (USBD_ERR(r, USBD_NO_DEVICE) || ++fails > 50) {
            log_line("usb_pad: %s: read %#x, giving up", d->path, (unsigned)r);
            d->gone = 1;
            break;
        }
        if (fails <= 3) log_line("usb_pad: %s: read %#x", d->path, (unsigned)r);
        usleep(20000);
    }
    return NULL;
}

static void dev_close(usb_dev_t *d)
{
    if (d->thread_on) {
        d->quit = 1;
        pthread_join(d->thread, NULL);
        d->thread_on = 0;
        pthread_mutex_destroy(&d->lock);
    }
    if (d->h) {
        U.release(d->h, d->iface);
        U.close(d->h);
    }
    d->h = NULL;
}

/* Opens `dev`, finds its controller interface and starts reading it. */
static int probe_usbd(usb_dev_t *d, usbd_dev *dev)
{
    static uint8_t cfg[1024];
    uint8_t dd[64];
    usb_iface ifs[8];
    int r, len, nifs;

    memset(dd, 0, sizeof dd);
    if ((r = U.get_desc(dev, dd)) < 0) return 0;
    d->vid = le16(dd + 8);
    d->pid = le16(dd + 10);
    if (dd[4] == 0x09) return 0;                    /* hub */
    if (d->vid == 0x054C) {
        log_line("usb_pad: %s (%04x:%04x): Sony pad, left to the console", d->path, d->vid, d->pid);
        return 0;
    }
    if ((r = U.open(dev, &d->h)) < 0) {
        log_line("usb_pad: %s (%04x:%04x): cannot open: %#x", d->path, d->vid, d->pid, (unsigned)r);
        d->h = NULL;
        return 0;
    }
    len = U.ctrl(d->h, 0x80, 0x06, 0x0200, 0, cfg, sizeof cfg, 500);
    if (len <= 0) {
        log_line("usb_pad: %s (%04x:%04x): no configuration descriptor: %#x", d->path, d->vid, d->pid, (unsigned)len);
        dev_close(d);
        return 0;
    }
    log_ifaces(d->path, cfg, len);
    nifs = read_ifaces(cfg, len, ifs, 8);
    if (!choose_iface(d, ifs, nifs)) {
        log_line("usb_pad: %s (%04x:%04x): no controller interface, left alone", d->path, d->vid, d->pid);
        dev_close(d);
        return 0;
    }
    if (U.driver_active(d->h, d->iface) == 1) {
        r = U.detach(d->h, d->iface);
        log_line("usb_pad: %s: interface %d taken from the system driver: %#x", d->path, d->iface, (unsigned)r);
    }
    /* A licensed pad (GameSir Kaleid Flux) also has USB audio interfaces: left
     * alone, the console takes them for a headset and routes sound there,
     * silencing the TV. Claim them so no sound is sent to the pad. */
    {
        int off = 0;

        while (off + 2 <= len) {
            const uint8_t *dsc = cfg + off;

            if (dsc[0] < 2 || off + dsc[0] > len) break;
            if (dsc[1] == 0x04 && dsc[0] >= 9 && dsc[3] == 0 && dsc[5] == 0x01 && dsc[2] != d->iface) {
                if (U.driver_active(d->h, dsc[2]) == 1) U.detach(d->h, dsc[2]);
                r = U.claim(d->h, dsc[2]);
                log_line("usb_pad: %s: audio interface %d claimed (no headset): %#x", d->path, dsc[2], (unsigned)r);
            }
            off += dsc[0];
        }
    }
    /* Windows asks every new device for the Microsoft OS string (0xEE); pads
     * that pick their mode by the host take that as a PC and stay in their
     * Xbox mode. The answer is not needed. */
    if (d->kind != KIND_HID) {
        uint8_t os[18];

        r = U.ctrl(d->h, 0x80, 0x06, 0x03EE, 0, os, sizeof os, 500);
        log_line("usb_pad: %s: Microsoft OS string asked: %d", d->path, r);
    }
    /* An XInput or GIP pad's HID interfaces (the G8 Plus brings a keyboard)
     * are the same pad: claimed, so the console does not add a keyboard. */
    if (d->kind != KIND_HID) {
        int k;

        for (k = 0; k < nifs; k++) {
            if (ifs[k].kind != KIND_HID) continue;
            if (U.driver_active(d->h, ifs[k].iface) == 1) U.detach(d->h, ifs[k].iface);
            r = U.claim(d->h, ifs[k].iface);
            log_line("usb_pad: %s: extra HID interface %d claimed: %#x", d->path, ifs[k].iface, (unsigned)r);
        }
    }
    if ((r = U.claim(d->h, d->iface)) < 0) {
        log_line("usb_pad: %s: interface %d not claimed: %#x", d->path, d->iface, (unsigned)r);
        U.close(d->h);
        d->h = NULL;
        return 0;
    }
    pthread_mutex_init(&d->lock, NULL);
    if (pthread_create(&d->thread, NULL, reader, d) != 0) {
        log_line("usb_pad: %s: no reader thread", d->path);
        pthread_mutex_destroy(&d->lock);
        dev_close(d);
        return 0;
    }
    d->thread_on = 1;
    return 1;
}

static void scan_usb(usb_pad_mgr_t *mgr)
{
    char keys[32][32];
    usbd_dev **list = NULL;
    long n, i;
    int k, nkeys = 0;

    if (!U.ok) return;
    n = U.get_list(&list);
    if (n < 0) {
        log_line("usb_pad: device list %#lx", (unsigned long)n);
        return;
    }
    for (i = 0; i < n; i++) {
        usb_dev_t *d;
        char path[32];

        snprintf(path, sizeof path, "usb:%u.%u", U.bus(list[i]), U.addr(list[i]));
        if (nkeys < 32) snprintf(keys[nkeys++], sizeof keys[0], "%s", path);
        if (!(d = new_dev(mgr, path))) continue;
        if (!probe_usbd(d, list[i])) {
            mark_seen(mgr, path);
            continue;
        }
        d->active = 1;
        log_taken(d);
    }
    U.free_list(list, 1);

    /* What is no longer listed has gone; looked at again if it comes back. */
    for (i = 0; i < MAX_SEEN; i++) {
        int there = 0;

        if (!mgr->seen[i][0]) continue;
        for (k = 0; k < nkeys && !there; k++) there = strcmp(keys[k], mgr->seen[i]) == 0;
        if (!there) mgr->seen[i][0] = '\0';
    }
    for (i = 0; i < MAX_USB_DEVS; i++) {
        usb_dev_t *d = &mgr->devs[i];
        int there = 0;

        if (!d->active) continue;
        for (k = 0; k < nkeys && !there; k++) there = strcmp(keys[k], d->path) == 0;
        if (!there) drop(mgr, d, "disconnected");
    }
}

static void poll_dev(usb_pad_mgr_t *mgr, usb_dev_t *d, long now)
{
    uint8_t buf[REPORT_MAX];

    (void)now;
    for (;;) {
        int n = 0;

        pthread_mutex_lock(&d->lock);
        if (d->qcount > 0) {
            n = d->qlen[d->qhead];
            memcpy(buf, d->q[d->qhead], (size_t)n);
            d->qhead = (d->qhead + 1) % QUEUE_LEN;
            d->qcount--;
        }
        pthread_mutex_unlock(&d->lock);
        if (n <= 0) break;
        handle(mgr, d, buf, n);
    }
    if (d->gone) drop(mgr, d, "stopped answering");
}

static void backend_init(void)
{
    if (!usbd_load()) log_line("usb_pad: libSceUsbd not available, no USB pads");
}


/* ---- the manager ---------------------------------------------------------- */

usb_pad_mgr_t *usb_pad_init(usb_pad_feed_fn on_feed,
                             usb_pad_attach_fn on_attach,
                             usb_pad_detach_fn on_detach)
{
    usb_pad_mgr_t *mgr = calloc(1, sizeof(*mgr));

    if (!mgr) return NULL;
    backend_init();
    mgr->on_feed = on_feed;
    mgr->on_attach = on_attach;
    mgr->on_detach = on_detach;
    mgr->last_scan = now_ms();
    scan_usb(mgr);
    return mgr;
}

void usb_pad_poll(usb_pad_mgr_t *mgr, long now)
{
    int i;

    if (!mgr) return;
    if (now - mgr->last_scan >= USB_SCAN_INTERVAL_MS) {
        mgr->last_scan = now;
        scan_usb(mgr);
    }
    for (i = 0; i < MAX_USB_DEVS; i++)
        if (mgr->devs[i].active) poll_dev(mgr, &mgr->devs[i], now);
}

void usb_pad_close(usb_pad_mgr_t *mgr)
{
    int i;

    if (!mgr) return;
    for (i = 0; i < MAX_USB_DEVS; i++)
        if (mgr->devs[i].active) drop(mgr, &mgr->devs[i], "closed");
    free(mgr);
}
