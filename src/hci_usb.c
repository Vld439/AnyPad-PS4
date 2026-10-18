#include "evstream.h"
#include "hci_usb.h"
#include "log.h"
#include "usb_desc.h"
#include "util.h"

#include <sys/ioctl.h>
#include <sys/types.h>

#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Where the Bluetooth chip may be: ugen0.2 on every console seen so far
 * (MediaTek and Marvell/NXP alike); the others as programs that run on
 * both chips also look. */
static const char *const k_nodes[] = { "/dev/ugen0.2", "/dev/ugen0.3", "/dev/ugen1.2", "/dev/ugen0.1" };
#define NODE        (g_ctx.node[0] ? g_ctx.node : k_nodes[0])

/* The endpoints are read from the chip's configuration descriptor. These are
 * a layout used only if the descriptor cannot be read: on a PS5 fat with
 * firmware 10.01 it is wrong (events come on
 * 0x81 and ACL on 0x82), which is why the descriptor is trusted instead. */
#define EP_EVENTS_DEFAULT   0x82
#define EP_ACL_IN_DEFAULT   0x81
#define EP_ACL_OUT_DEFAULT  0x01
#define MPS_EVENTS_DEFAULT  16

/* Reads kept pending to win packets from the system's driver. The kernel
 * caps the transfers one process may have open (around 65), so events get
 * buffers sized for the largest event and ACL reads take the rest. */
#define N_EVENT_RD  40
#define N_ACL_RD    20
#define ACL_BUF     HCI_PKT_MAX

#define X_ACL_OUT   (N_EVENT_RD + N_ACL_RD)
#define X_ACL_OUT2  (X_ACL_OUT + 1)     /* the chip's second bulk OUT, tried if the first never finishes */
#define N_XFER      (X_ACL_OUT2 + 1)

#define RING        64

typedef struct {
    unsigned char pkt[RING][HCI_PKT_MAX];
    int len[RING];
    int head, count;
} ring;

typedef struct {
    int fd;
    struct usb_fs_endpoint ep[N_XFER];
    void *bufp[N_XFER][1];
    uint32_t lenp[N_XFER][1];
    unsigned char buf[N_XFER][ACL_BUF];
    unsigned char busy[N_XFER];
    int n_event, n_acl;             /* reads actually opened */
    long t_topup;                   /* when the missing reads were last tried again */
    int dead;                       /* the device went away */
    ring events, acl;

    /* The HCI function in use and its endpoints. */
    int iface;                      /* its interface number: where commands go */
    uint8_t ep_events, ep_acl_in, ep_acl_out, ep_acl_out2;
    int out_idx;                    /* the OUT transfer in use: X_ACL_OUT or X_ACL_OUT2 */
    int out2_open;
    int out_done_logged;
    int ev_read_len;                /* one packet of the events endpoint */
    int n_functions;                /* how many the chip has, for the log */
    evstream es;
    long yield_until;               /* event reads left to the system until then */
    int ev_want;                    /* event reads to keep pending (dialog mode: fewer when idle) */
    int ev_len;                     /* length asked of each event read (dialog mode) */

    char node[16];                  /* the device node in use */

    /* What was seen, for diag(). */
    struct usb_device_descriptor dd;
    int dd_ok;
    unsigned reads_done, reads_empty, status_hist[48];
    unsigned events_seen, acl_seen;
    unsigned char sample[12][24];
    int sample_len[12], n_sample;
    unsigned char last_cmd[40];
    int last_cmd_len;
    int first_idx, first_len, first_frames;     /* the first read to finish */
    unsigned char first_bytes[16];
} usb_ctx;

static usb_ctx g_ctx;

/* ---- packet rings -------------------------------------------------------- */

static void ring_put(ring *r, const unsigned char *p, int n)
{
    if (n > HCI_PKT_MAX) n = HCI_PKT_MAX;
    if (r->count == RING) {             /* full: the oldest goes */
        r->head = (r->head + 1) % RING;
        r->count--;
    }
    {
        int slot = (r->head + r->count) % RING;
        memcpy(r->pkt[slot], p, (size_t)n);
        r->len[slot] = n;
        r->count++;
    }
}

static int ring_get(ring *r, unsigned char *out, int max)
{
    int n;

    if (!r->count) return 0;
    n = r->len[r->head] < max ? r->len[r->head] : max;
    memcpy(out, r->pkt[r->head], (size_t)n);
    r->head = (r->head + 1) % RING;
    r->count--;
    return n;
}

/* ---- transfers ----------------------------------------------------------- */

static int xfer_open_quiet(usb_ctx *c, int index, int ep_addr, int size)
{
    struct usb_fs_open op;

    memset(&op, 0, sizeof op);
    op.ep_index = (uint8_t)index;
    op.ep_no = (uint8_t)ep_addr;
    op.max_bufsize = (uint32_t)size;
    op.max_frames = 1;
    return ioctl(c->fd, USB_FS_OPEN, &op) == 0;
}

static int xfer_open(usb_ctx *c, int index, int ep_addr, int size)
{
    struct usb_fs_open op;

    memset(&op, 0, sizeof op);
    op.ep_index = (uint8_t)index;
    op.ep_no = (uint8_t)ep_addr;
    op.max_bufsize = (uint32_t)size;
    op.max_frames = 1;
    if (ioctl(c->fd, USB_FS_OPEN, &op) != 0) {
        log_line("usb: open endpoint %#x as transfer %d: errno %d", ep_addr, index, errno);
        return 0;
    }
    return 1;
}

static int xfer_start(usb_ctx *c, int index, int length)
{
    struct usb_fs_start st;

    c->lenp[index][0] = (uint32_t)length;
    c->ep[index].nFrames = 1;
    c->ep[index].aFrames = 0;
    c->ep[index].status = 0;
    memset(&st, 0, sizeof st);
    st.ep_index = (uint8_t)index;
    if (ioctl(c->fd, USB_FS_START, &st) != 0) {
        log_line("usb: start transfer %d: errno %d", index, errno);
        if (errno == ENXIO || errno == ENODEV || errno == EIO || errno == EBADF) c->dead = 1;
        return 0;
    }
    c->busy[index] = 1;
    return 1;
}

/* A finished read ends on a short packet, so it holds whole HCI packets
 * unless the system's read took the start of one: a trailing part that
 * does not add up is dropped. */
static void event_done(void *ctx, const uint8_t *ev, int len)
{
    usb_ctx *c = ctx;

    c->events_seen++;
    if (c->n_sample < 12) {
        int k = len < 24 ? len : 24;
        memcpy(c->sample[c->n_sample], ev, (size_t)k);
        c->sample_len[c->n_sample++] = k;
    }
    ring_put(&c->events, ev, len);
}

static void split_read(usb_ctx *c, int index)
{
    const unsigned char *p = c->buf[index];
    int n = (int)c->lenp[index][0], off = 0;

    if (index < N_EVENT_RD) {           /* a piece of the event stream */
        evstream_feed(&c->es, p, n, now_ms(), event_done, c);
        return;
    }
    while (off + 4 <= n) {              /* ACL: whole packets */
        int total = 4 + (int)le16(p + off + 2);
        if (off + total > n) break;
        c->acl_seen++;
        ring_put(&c->acl, p + off, total);
        off += total;
    }
}

static int reap(usb_ctx *c)
{
    struct usb_fs_complete done;
    int any = 0;

    for (;;) {
        memset(&done, 0, sizeof done);
        if (ioctl(c->fd, USB_FS_COMPLETE, &done) != 0) break;
        any = 1;
        if (done.ep_index >= N_XFER) continue;
        c->busy[done.ep_index] = 0;
        if (done.ep_index == X_ACL_OUT || done.ep_index == X_ACL_OUT2) {
            if (c->ep[done.ep_index].status)
                log_line("usb: ACL out status %u", (unsigned)c->ep[done.ep_index].status);
            else if (!c->out_done_logged) {
                c->out_done_logged = 1;
                log_line("usb: first ACL packet written on endpoint %#x",
                         done.ep_index == X_ACL_OUT ? c->ep_acl_out : c->ep_acl_out2);
            }
            continue;
        }
        if (c->reads_done == 0) {
            int n = (int)c->lenp[done.ep_index][0];
            c->first_idx = done.ep_index;
            c->first_len = n;
            c->first_frames = c->ep[done.ep_index].aFrames;
            memcpy(c->first_bytes, c->buf[done.ep_index], n < 16 ? (size_t)n : 16);
        }
        c->reads_done++;
        {
            unsigned st = (unsigned)c->ep[done.ep_index].status;
            c->status_hist[st < 47 ? st : 47]++;
        }
        if (c->ep[done.ep_index].status == 0 && c->ep[done.ep_index].aFrames) split_read(c, done.ep_index);
        else c->reads_empty++;
    }
    return any;
}

/* If the controller was opened with fewer reads than wanted (another payload
 * still held some of its transfers), the missing ones are asked for again
 * every few seconds: when the other one lets go, this one recovers by itself
 * instead of running thin until it is restarted. */
static void topup_reads(usb_ctx *c)
{
    long now = now_ms();
    int before_e = c->n_event, before_a = c->n_acl;

    if (c->n_event >= N_EVENT_RD && c->n_acl >= N_ACL_RD) return;
    if (now - c->t_topup < 5000) return;
    c->t_topup = now;
    while (c->n_event < N_EVENT_RD && xfer_open_quiet(c, c->n_event, c->ep_events, ACL_BUF)) c->n_event++;
    while (c->n_acl < N_ACL_RD && xfer_open_quiet(c, N_EVENT_RD + c->n_acl, c->ep_acl_in, ACL_BUF)) c->n_acl++;
    if (c->n_event != before_e || c->n_acl != before_a)
        log_line("usb: more reads opened: %d event and %d ACL (wanted %d and %d)", c->n_event, c->n_acl,
                 N_EVENT_RD, N_ACL_RD);
}

static int op_missing_reads(void *ctx)
{
    usb_ctx *c = ctx;

    return (N_EVENT_RD - c->n_event) + (N_ACL_RD - c->n_acl);
}

static void keep_reads_pending(usb_ctx *c)
{
    int i;

    if (now_ms() >= c->yield_until) topup_reads(c);
    for (i = 0; i < c->n_event && i < c->ev_want && now_ms() >= c->yield_until; i++)
        if (!c->busy[i]) xfer_start(c, i, c->ev_len);
    for (i = 0; i < c->n_acl; i++)
        if (!c->busy[N_EVENT_RD + i]) xfer_start(c, N_EVENT_RD + i, ACL_BUF);
}

/* ---- hci_ops ------------------------------------------------------------- */

static int op_cmd(void *ctx, unsigned opcode, const void *params, int plen)
{
    usb_ctx *c = ctx;
    struct usb_ctl_request req;
    unsigned char pkt[3 + 255];

    if (plen < 0 || plen > 255) return 0;
    put16(pkt, opcode);
    pkt[2] = (unsigned char)plen;
    if (plen) memcpy(pkt + 3, params, (size_t)plen);
    c->last_cmd_len = 3 + plen < (int)sizeof c->last_cmd ? 3 + plen : (int)sizeof c->last_cmd;
    memcpy(c->last_cmd, pkt, (size_t)c->last_cmd_len);

    memset(&req, 0, sizeof req);
    req.ucr_data = pkt;
    req.ucr_request.bmRequestType = UT_WRITE_CLASS_DEVICE;
    req.ucr_request.bRequest = 0;
    USETW(req.ucr_request.wValue, 0);
    USETW(req.ucr_request.wIndex, c->iface);
    USETW(req.ucr_request.wLength, 3 + plen);
    if (ioctl(c->fd, USB_DO_REQUEST, &req) != 0) {
        log_line("usb: command %#06x: errno %d", opcode, errno);
        if (errno == ENXIO || errno == ENODEV || errno == EIO || errno == EBADF) c->dead = 1;
        return 0;
    }
    return 1;
}

static int op_acl_send(void *ctx, const unsigned char *pkt, int len)
{
    usb_ctx *c = ctx;
    long deadline = now_ms() + 500;

    if (len > ACL_BUF) return 0;
    while (c->busy[c->out_idx]) {
        reap(c);
        if (!c->busy[c->out_idx]) break;
        if (now_ms() >= deadline) {
            /* The write never finished: the chip may take ACL on its other
             * bulk OUT endpoint. Try it once, and say so. */
            if (c->out_idx == X_ACL_OUT && c->out2_open) {
                struct usb_fs_stop st;

                memset(&st, 0, sizeof st);
                st.ep_index = (uint8_t)X_ACL_OUT;
                ioctl(c->fd, USB_FS_STOP, &st);
                c->busy[X_ACL_OUT] = 0;
                c->out_idx = X_ACL_OUT2;
                log_line("usb: ACL out %#x never finished; switching to %#x", c->ep_acl_out, c->ep_acl_out2);
                break;
            }
            log_line("usb: ACL out still busy");
            return 0;
        }
        usleep(500);
    }
    memcpy(c->buf[c->out_idx], pkt, (size_t)len);
    return xfer_start(c, c->out_idx, len);
}

static int op_pump(void *ctx, int timeout_ms)
{
    usb_ctx *c = ctx;
    long deadline = now_ms() + timeout_ms;

    for (;;) {
        int any;

        if (c->dead) return -1;
        keep_reads_pending(c);
        any = reap(c);
        if (c->events.count || c->acl.count) return 1;
        if (any) continue;
        if (now_ms() >= deadline) return 0;
        usleep(1000);
    }
}

static void op_event_len(void *ctx, int n)
{
    usb_ctx *c = ctx;
    struct usb_fs_stop st;
    int i;

    if (n < 8) n = 8;
    if (n > ACL_BUF) n = ACL_BUF;
    if (n == c->ev_len) return;
    c->ev_len = n;
    for (i = 0; i < c->n_event; i++) {         /* started again with the new length */
        if (!c->busy[i]) continue;
        memset(&st, 0, sizeof st);
        st.ep_index = (uint8_t)i;
        ioctl(c->fd, USB_FS_STOP, &st);
    }
    reap(c);
    log_line("usb: event reads of %d bytes", n);
}

static void op_event_reads(void *ctx, int n)
{
    usb_ctx *c = ctx;
    struct usb_fs_stop st;
    int i;

    if (n < 0) n = 0;
    if (n > N_EVENT_RD) n = N_EVENT_RD;
    if (n == c->ev_want) return;
    c->ev_want = n;
    for (i = n; i < c->n_event; i++) {
        if (!c->busy[i]) continue;
        memset(&st, 0, sizeof st);
        st.ep_index = (uint8_t)i;
        ioctl(c->fd, USB_FS_STOP, &st);
    }
    reap(c);
}

/* Leaves the events to the system's driver for a while: our pending event
 * reads are stopped and not renewed until then, so the system's own read is
 * the only one in line (a DualSense reconnecting to the console pages this
 * controller, and only the system can answer it). */
static void op_yield(void *ctx, int ms)
{
    usb_ctx *c = ctx;
    struct usb_fs_stop st;
    int i;

    c->yield_until = now_ms() + ms;
    for (i = 0; i < c->n_event; i++) {
        if (!c->busy[i]) continue;
        memset(&st, 0, sizeof st);
        st.ep_index = (uint8_t)i;
        ioctl(c->fd, USB_FS_STOP, &st);
    }
    reap(c);
    log_line("usb: event reads left to the system for %d ms", ms);
}

static int op_next_event(void *ctx, unsigned char *out, int max)
{
    return ring_get(&((usb_ctx *)ctx)->events, out, max);
}

static int op_next_acl(void *ctx, unsigned char *out, int max)
{
    return ring_get(&((usb_ctx *)ctx)->acl, out, max);
}

static void op_close(void *ctx)
{
    usb_ctx *c = ctx;
    struct usb_fs_uninit un;

    if (c->fd < 0) return;
    {
        /* Stop every pending transfer first, so that releasing them cannot
         * wait on reads that would only finish with more traffic. */
        struct usb_fs_stop st;
        int i;
        for (i = 0; i < N_XFER; i++) {
            if (!c->busy[i]) continue;
            memset(&st, 0, sizeof st);
            st.ep_index = (uint8_t)i;
            ioctl(c->fd, USB_FS_STOP, &st);
            c->busy[i] = 0;
        }
    }
    memset(&un, 0, sizeof un);
    ioctl(c->fd, USB_FS_UNINIT, &un);
    close(c->fd);
    c->fd = -1;
}

/* Why the controller may not be answering: what the device is, whether
 * anything at all came in, in which state the transfers finished, and the
 * first events the system's own traffic put in front of us. */
static void op_diag(void *ctx)
{
    usb_ctx *c = ctx;
    char hex[3 * 24 + 1];
    unsigned i;
    int k, j;

    if (c->dd_ok)
        log_line("diag: device %04x:%04x class %02x/%02x/%02x, USB %04x, %d endpoint-0 bytes",
                 UGETW(c->dd.idVendor), UGETW(c->dd.idProduct), c->dd.bDeviceClass,
                 c->dd.bDeviceSubClass, c->dd.bDeviceProtocol, UGETW(c->dd.bcdUSB),
                 c->dd.bMaxPacketSize);
    else
        log_line("diag: the device descriptor could not be read");
    log_line("diag: %u reads finished (%u empty or failed); %u events and %u ACL packets seen",
             c->reads_done, c->reads_empty, c->events_seen, c->acl_seen);
    if (c->reads_done) {
        for (k = 0; k < c->first_len && k < 16; k++) snprintf(hex + 3 * k, 4, "%02x ", c->first_bytes[k]);
        if (c->first_len <= 0) hex[0] = '\0';
        log_line("diag: the first read to finish was transfer %d (%s), %d bytes in %d frame(s): %s",
                 c->first_idx, c->first_idx < N_EVENT_RD ? "events" : "ACL in",
                 c->first_len, c->first_frames, hex);
        log_line("diag: using interface %d: events %#x, ACL in %#x, ACL out %#x (%d HCI function(s) in the chip)",
                 c->iface, c->ep_events, c->ep_acl_in, c->ep_acl_out, c->n_functions);
    }
    for (i = 0; i < 48; i++)
        if (c->status_hist[i]) log_line("diag: %u reads ended with USB status %u", c->status_hist[i], i);
    for (j = 0; j < c->n_sample; j++) {
        for (k = 0; k < c->sample_len[j]; k++) snprintf(hex + 3 * k, 4, "%02x ", c->sample[j][k]);
        log_line("diag: event %d: %s", j + 1, hex);
    }
    for (k = 0; k < c->last_cmd_len && k < 24; k++) snprintf(hex + 3 * k, 4, "%02x ", c->last_cmd[k]);
    if (c->last_cmd_len) log_line("diag: last command sent: %s", hex);

    /* Read-only questions about the device: what it calls itself, its whole
     * configuration (every interface and endpoint), and which kernel driver
     * has each interface. This is what shows how this firmware lays the chip
     * out. Nothing here sends anything to it or takes anything from it. */
    {
        struct usb_device_info di;
        struct usb_gen_descriptor gd;
        unsigned char cfg[512];
        char name[64];

        memset(&di, 0, sizeof di);
        if (ioctl(c->fd, USB_GET_DEVICEINFO, &di) == 0)
            log_line("diag: \"%.40s\" by \"%.40s\", bus %u address %u, config %u, speed %u, power %u mA, suspended %u",
                     di.udi_product, di.udi_vendor, di.udi_bus, di.udi_addr, di.udi_config_no,
                     di.udi_speed, di.udi_power, di.udi_suspended);

        memset(&gd, 0, sizeof gd);
        gd.ugd_data = cfg;
        gd.ugd_maxlen = sizeof cfg;
        gd.ugd_config_index = 0xFF;                 /* the current configuration */
        if (ioctl(c->fd, USB_GET_FULL_DESC, &gd) == 0) {
            int total = gd.ugd_actlen < (int)sizeof cfg ? gd.ugd_actlen : (int)sizeof cfg;
            log_line("diag: configuration descriptor, %d bytes", total);
            for (j = 0; j < total; j += 24) {
                char line[3 * 24 + 1];
                int m = total - j < 24 ? total - j : 24;
                for (k = 0; k < m; k++) snprintf(line + 3 * k, 4, "%02x ", cfg[j + k]);
                log_line("diag: cfg[%03d] %s", j, line);
            }
        } else {
            log_line("diag: the configuration descriptor could not be read (errno %d)", errno);
        }

        for (j = 0; j < 6; j++) {
            memset(&gd, 0, sizeof gd);
            memset(name, 0, sizeof name);
            gd.ugd_data = name;
            gd.ugd_maxlen = sizeof name - 1;
            gd.ugd_iface_index = (uint8_t)j;
            if (ioctl(c->fd, USB_GET_IFACE_DRIVER, &gd) == 0)
                log_line("diag: interface %d is held by driver \"%.40s\"", j, name);
        }
    }
}

/* The HCI function and endpoints to use, from the chip's own descriptor: the
 * first function in descriptor order. (The second one is where the system
 * runs the DualSense; the first was silent in every trial.) */
static int choose_endpoints(usb_ctx *c)
{
    struct usb_gen_descriptor gd;
    unsigned char cfg[512];
    hci_function f[HCI_FUNCTIONS_MAX];
    int n = 0, i;

    memset(&gd, 0, sizeof gd);
    gd.ugd_data = cfg;
    gd.ugd_maxlen = sizeof cfg;
    gd.ugd_config_index = 0xFF;                 /* the current configuration */
    if (ioctl(c->fd, USB_GET_FULL_DESC, &gd) == 0)
        n = usb_find_hci_functions(cfg, gd.ugd_actlen < (int)sizeof cfg ? gd.ugd_actlen : (int)sizeof cfg, f);
    c->n_functions = n;
    if (n > 0) {
        for (i = 0; i < n; i++)
            log_line("usb: HCI function %d: interface %d, events %#x (%u bytes), ACL in %#x, ACL out %#x",
                     i, f[i].iface, f[i].ep_events, f[i].mps_events, f[i].ep_acl_in, f[i].ep_acl_out);
        c->iface = f[0].iface;
        c->ep_events = f[0].ep_events;
        c->ep_acl_in = f[0].ep_acl_in;
        c->ep_acl_out = f[0].ep_acl_out;
        c->ep_acl_out2 = usb_spare_acl_out(&f[0]);
        c->ev_read_len = f[0].mps_events >= 8 && f[0].mps_events <= 64 ? f[0].mps_events : MPS_EVENTS_DEFAULT;
        return n;
    }
    return 0;
}

#if !defined(TARGET_PS4)
static void default_endpoints(usb_ctx *c)
{
    log_line("usb: no HCI function found in the descriptor; using the default layout");
    c->iface = 0;
    c->ep_events = EP_EVENTS_DEFAULT;
    c->ep_acl_in = EP_ACL_IN_DEFAULT;
    c->ep_acl_out = EP_ACL_OUT_DEFAULT;
    c->ev_read_len = MPS_EVENTS_DEFAULT;
}
#endif

static int op_chip_vendor(void *ctx);

static const hci_ops k_usb_ops = {
    op_cmd, op_acl_send, op_pump, op_next_event, op_next_acl, op_close, op_diag, op_yield, op_missing_reads,
    op_event_reads, op_event_len, op_chip_vendor,
};

/* Opens one device node and reads its device descriptor. */
static int open_node(usb_ctx *c, const char *path, int log_failure)
{
    snprintf(c->node, sizeof c->node, "%s", path);
    c->fd = open(path, O_RDWR);
    if (c->fd < 0) {
        if (log_failure) log_line("usb: open %s: errno %d", path, errno);
        return 0;
    }
    c->dd_ok = ioctl(c->fd, USB_GET_DEVICE_DESC, &c->dd) == 0;
    if (c->dd_ok)
        log_line("usb: %s is %04x:%04x", path, UGETW(c->dd.idVendor), UGETW(c->dd.idProduct));
    return 1;
}

static int g_chip_vendor;
static char g_chip_name[40];

/* Which chip this is: it decides what is safe to change on it. */
static void log_chip(usb_ctx *c)
{
    unsigned vid = UGETW(c->dd.idVendor);

    g_chip_vendor = (int)vid;
    snprintf(g_chip_name, sizeof g_chip_name, "%s (%04x:%04x)", vid == 0x0E8D ? "MediaTek" : vid == 0x1286 ? "Marvell/NXP" : "USB",
             vid, UGETW(c->dd.idProduct));
    if (vid == 0x0E8D)
        log_line("chip: MediaTek (%04x:%04x)", vid, UGETW(c->dd.idProduct));
    else if (vid == 0x1286)
        log_line("chip: Marvell/NXP (%04x:%04x): the system's page scan is left as it is", vid, UGETW(c->dd.idProduct));
    else
        log_line("chip: %04x:%04x, not seen before", vid, UGETW(c->dd.idProduct));
}

int hci_usb_chip_vendor(void) { return g_chip_vendor; }
const char *hci_usb_chip_name(void) { return g_chip_name; }

static int op_chip_vendor(void *ctx) { (void)ctx; return g_chip_vendor; }

int hci_usb_open(hci_t *out)
{
    usb_ctx *c = &g_ctx;
    struct usb_fs_init init;
    int i;
    size_t k;

    memset(c, 0, sizeof *c);
    c->fd = -1;
    /* The first node that is a Bluetooth chip (an HCI function in its
     * descriptor); if none is, ugen0.2 with the default layout, as before. */
    for (k = 0; k < sizeof k_nodes / sizeof k_nodes[0] && c->fd < 0; k++) {
        if (!open_node(c, k_nodes[k], k == 0)) continue;
        if (choose_endpoints(c) > 0) break;
        close(c->fd);
        c->fd = -1;
    }
    if (c->fd < 0) {
#if !defined(TARGET_PS4)
        if (!open_node(c, k_nodes[0], 1)) return 0;
        default_endpoints(c);
#else
        return 0;
#endif
    }
    if (c->dd_ok) log_chip(c);
    evstream_init(&c->es);

    for (i = 0; i < N_XFER; i++) {
        c->bufp[i][0] = c->buf[i];
        c->lenp[i][0] = ACL_BUF;
        c->ep[i].ppBuffer = c->bufp[i];
        c->ep[i].pLength = c->lenp[i];
        c->ep[i].nFrames = 1;
        c->ep[i].flags = USB_FS_FLAG_SINGLE_SHORT_OK;
    }
    /* A bulk OUT transfer that is a multiple of the packet size must end
     * with a zero-length packet. */
    c->ep[X_ACL_OUT].flags = USB_FS_FLAG_FORCE_SHORT;
    c->ep[X_ACL_OUT2].flags = USB_FS_FLAG_FORCE_SHORT;
    c->out_idx = X_ACL_OUT;

    memset(&init, 0, sizeof init);
    init.pEndpoints = c->ep;
    init.ep_index_max = N_XFER;
    if (ioctl(c->fd, USB_FS_INIT, &init) != 0) {
        log_line("usb: init: errno %d", errno);
        op_close(c);
        return 0;
    }

    /* The OUT transfer first: if the kernel's budget runs out, it must not
     * be the one left without a transfer. */
    if (!xfer_open(c, X_ACL_OUT, c->ep_acl_out, ACL_BUF)) {
        op_close(c);
        return 0;
    }
    /* The spare OUT is opened only if it is there; its absence is no failure. */
    if (c->ep_acl_out2) c->out2_open = xfer_open(c, X_ACL_OUT2, c->ep_acl_out2, ACL_BUF);
    /* On this chip (MediaTek 0e8d:3603) the bulk OUT the descriptor names
     * first (0x01) takes HCI commands, not ACL: an ACL packet written there
     * runs as a command whose opcode is its header (handle 0x033 with its
     * flags reads as 0x2033), answered "unknown command" to whoever reads
     * events, the system's stack included. ACL goes to the other one (0x02)
     * from the start; 0x01 only if there is no other. */
    if (c->out2_open) c->out_idx = X_ACL_OUT2;
    c->ev_want = N_EVENT_RD;
    c->ev_len = c->ev_read_len;
    for (c->n_event = 0; c->n_event < N_EVENT_RD; c->n_event++)
        if (!xfer_open(c, c->n_event, c->ep_events, ACL_BUF)) break;
    for (c->n_acl = 0; c->n_acl < N_ACL_RD; c->n_acl++)
        if (!xfer_open(c, N_EVENT_RD + c->n_acl, c->ep_acl_in, ACL_BUF)) break;
    if (!c->n_event || !c->n_acl) {
        op_close(c);
        return 0;
    }

    log_line("usb: controller open beside the system: %d event reads, %d ACL reads",
             c->n_event, c->n_acl);
    out->ops = &k_usb_ops;
    out->ctx = c;
    return 1;
}
