#include "ps5_vpad.h"
#include "host.h"
#include "log.h"
#include "util.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---- ScePadData ------------------------------------------------------------
 *
 * As PS4/PS5 software sees it: 120 bytes, with acceleration before
 * angular velocity. */

typedef struct {
    uint16_t x, y;
    uint8_t  id;
    uint8_t  reserved[3];
} ScePadTouch;

typedef struct {
    uint8_t  touchNum;
    uint8_t  reserved[3];
    uint32_t reserved2;
    ScePadTouch touch[2];
} ScePadTouchData;

typedef struct {
    uint32_t buttons;
    uint8_t  lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  padding[2];
    float    orientation[4];        /* x, y, z, w */
    float    acceleration[3];       /* g */
    float    angularVelocity[3];    /* rad/s */
    ScePadTouchData touchData;
    uint8_t  connected;
    uint64_t timestamp;             /* microseconds */
    uint8_t  extensionUnitData[16];
    uint8_t  connectedCount;
    uint8_t  reserved[2];
    uint8_t  deviceUniqueDataLen;
    uint8_t  deviceUniqueData[12];
} ScePadData;

_Static_assert(sizeof(ScePadData) == 120, "ScePadData must be 120 bytes");
_Static_assert(offsetof(ScePadData, touchData) == 52, "touch data offset");
_Static_assert(offsetof(ScePadData, timestamp) == 80, "timestamp offset");

/* ---- system interfaces --------------------------------------------------- */

#define VIRTUAL_DEVICE_DUALSENSE 3

int32_t sceUserServiceInitialize(void *params);
int32_t sceUserServiceGetInitialUser(int32_t *user);
int32_t sceUserServiceGetForegroundUser(int32_t *user);
int32_t sceUserServiceGetLoginUserIdList(int32_t list[4]);
int32_t scePadInit(void);
int32_t scePadSetProcessPrivilege(int32_t privilege);
int32_t scePadGetHandle(int32_t user, int32_t type, int32_t index);
int32_t scePadOpen(int32_t user, int32_t type, int32_t index, const void *param);
int32_t scePadClose(int32_t handle);
int32_t scePadReadState(int32_t handle, ScePadData *data);
int32_t scePadVirtualDeviceAddDevice(void *param, int32_t deviceType);
int32_t scePadVirtualDeviceInsertData(int32_t handle, const void *data);
int32_t scePadVirtualDeviceDeleteDevice(int32_t handle);

/* From the payload SDK (ps5/kernel.h). */
uint64_t kernel_get_ucred_authid(pid_t pid);
int32_t  kernel_set_ucred_authid(pid_t pid, uint64_t authid);
int32_t  kernel_get_ucred_caps(pid_t pid, uint8_t caps[16]);
int32_t  kernel_set_ucred_caps(pid_t pid, const uint8_t caps[16]);

typedef int32_t (*mbus_bind_fn)(uint64_t device_id, int32_t user);

typedef struct {
    char reserved[45];
    char message[3075];
} notify_request;

int sceKernelSendNotificationRequest(int, notify_request *, size_t, int);

/* ---- state ------------------------------------------------------------------ */

#define T_IDENTIFY      3000    /* AddDevice to its DEVICE_ADDED line */
#define T_AMBIGUITY      300    /* after a match, watch for a second one */
#define LINE_MAX_LEN    1024

enum { VP_FREE, VP_QUEUED, VP_PENDING, VP_READY, VP_FAILED };

typedef struct {
    int st;
    int remove_when_found;      /* removed while being identified */
    int32_t handle;
    uint64_t dev;
    pad_state last;             /* the pad's latest state, resent while PS is held from the menu */
    long ps_until;              /* ms; PS is added to the frames until then (0: not held) */
    int ps_active;              /* a frame with PS went out and the release is still to be sent */
} vslot;

static mbus_bind_fn g_bind;
static int g_ready;                         /* vpad_init succeeded */
static int32_t g_user = -1;
static vslot g_slot[HOST_MAX_PADS];
static void ps_tick(long now);

/* The identification under way: one at a time, so a DEVICE_ADDED line can
 * only belong to the device just asked for. */
static int g_pending = -1;
static long g_t_add, g_t_match;
static int g_matches;
static uint64_t g_match_dev;
static int g_log_fd = -1, g_log_is_dev;
static char g_line[LINE_MAX_LEN];
static size_t g_line_len;

/* Raw sensor counts to physical units, uncalibrated: Sony pads report
 * 8192 counts per g and 1024 per degree per second. */
#define ACCEL_PER_G      8192.0f
#define GYRO_PER_DEG_S   1024.0f
#define DEG_TO_RAD       0.01745329252f

void notify(const char *fmt, ...)
{
    notify_request req;
    va_list ap;

    memset(&req, 0, sizeof req);
    va_start(ap, fmt);
    vsnprintf(req.message, sizeof req.message, fmt, ap);
    va_end(ap);
    sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}

/* ---- the kernel log ----------------------------------------------------------
 *
 * The device a virtual DualSense becomes is only told in the kernel log. The
 * log has one reader, which usually belongs to the HEN's log server; that
 * server re-serves it to any number of clients on TCP 3232. So:
 *   - the server is used when there is one;
 *   - /dev/klog only when there is none, held just for the seconds of one
 *     identification, and every line read is copied into AnyPad PS5's log so
 *     nothing taken from it is lost;
 *   - if neither answers with the expected lines, nothing is bound. */

static int log_source_open(void)
{
    struct sockaddr_in sin;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd >= 0) {
        memset(&sin, 0, sizeof sin);
        sin.sin_family = AF_INET;
        sin.sin_port = htons(3232);
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");
        if (connect(fd, (struct sockaddr *)&sin, sizeof sin) == 0) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
            g_log_is_dev = 0;
            return fd;
        }
        close(fd);
    }
    fd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        g_log_is_dev = 1;
        return fd;
    }
    log_line("vpad: no kernel log (no server on 3232, /dev/klog errno %d)", errno);
    return -1;
}

static void log_source_close(void)
{
    if (g_log_fd >= 0) close(g_log_fd);
    g_log_fd = -1;
    g_line_len = 0;
}

static uint64_t hex_after(const char *s, const char *key)
{
    const char *p = strstr(s, key);
    uint64_t v = 0;
    int digits = 0;

    if (!p) return 0;
    p += strlen(key);
    while (*p == ':' || *p == '=' || *p == ' ') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    while (isxdigit((unsigned char)*p) && digits++ < 16) {
        char c = *p++;
        v = v << 4 | (uint64_t)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}

/* A virtual DualSense appearing:
 *   SCE_MBUS_EVENT_DEVICE_ADDED [DeviceId:0x...][type:1][subType:22]
 *   [capabilityBattery:0]...
 * A real pad reports a battery, so a line saying otherwise is not ours. */
static uint64_t virtual_device_in(const char *line)
{
    const char *bat;

    if (!strstr(line, "DEVICE_ADDED") || !strstr(line, "type:1") || !strstr(line, "subType:22"))
        return 0;
    bat = strstr(line, "capabilityBattery:");
    if (bat && bat[18] != '0') return 0;
    return hex_after(line, "DeviceId");
}

/* Reads what the log has, a bounded amount per call. */
static void log_source_read(void)
{
    char buf[1024];
    int rounds;

    for (rounds = 0; rounds < 8 && g_log_fd >= 0; rounds++) {
        ssize_t n = read(g_log_fd, buf, sizeof buf), i;
        if (n <= 0) return;
        for (i = 0; i < n; i++) {
            uint64_t dev;
            if (buf[i] != '\n' && g_line_len < sizeof g_line - 1) {
                g_line[g_line_len++] = buf[i];
                continue;
            }
            g_line[g_line_len] = '\0';
            g_line_len = 0;
            if (g_log_is_dev) log_line("klog: %s", g_line);  /* kept, not lost */
            dev = virtual_device_in(g_line);
            if (!dev || g_pending < 0) continue;
            if (g_matches == 0) {
                g_match_dev = dev;
                g_t_match = now_ms();
            }
            if (g_matches == 0 || dev != g_match_dev) g_matches++;
        }
    }
}

/* ---- users and privileges ---------------------------------------------------- */

/* The user to give the pads to: one who is signed in. scePadOpen and the
 * bind answer 0x809b0081 (USER_NOT_LOGIN) for a user who is not, and the
 * foreground or initial user is not necessarily signed in (the first trial
 * of the pad hotkey on a console got exactly that). By default the initial
 * user, who is the one who owns the console, is preferred if signed in; then
 * the foreground user; then the first signed-in one.
 *
 * `want` (from /data/anypad/bind_user) overrides that for an experiment: a
 * game only sees a pad bound to the user who started it, so the pad can be
 * given to another signed-in user. "other" means a signed-in user who is not
 * the initial one; a hexadecimal id means that user. If the wish cannot be met
 * (nobody else signed in, or that id is not signed in) the default applies. */
int32_t vpad_choose_user(const int32_t login[4], int32_t init, int32_t fore, const char *want)
{
    int i, n = 0;

    for (i = 0; i < 4; i++) if (login[i] != -1 && login[i] != 0) n++;
    if (n == 0) return -1;                                       /* nobody is signed in */

    if (want && *want) {
        if (strcmp(want, "other") == 0) {
            for (i = 0; i < 4; i++)
                if (login[i] != -1 && login[i] != 0 && login[i] != init) return login[i];
        } else {
            char *end;
            unsigned long id = strtoul(want, &end, 16);
            if (end != want && *end == '\0')
                for (i = 0; i < 4; i++) if (login[i] != -1 && (uint32_t)login[i] == (uint32_t)id) return login[i];
        }
    }
    for (i = 0; i < 4; i++) if (init != -1 && login[i] == init) return init;
    for (i = 0; i < 4; i++) if (fore != -1 && login[i] == fore) return fore;
    for (i = 0; i < 4; i++) if (login[i] != -1 && login[i] != 0) return login[i];
    return -1;
}

static int32_t main_user(void)
{
    int32_t login[4] = { -1, -1, -1, -1 }, init = -1, fore = -1, seen[6];
    static int32_t logged[6] = { -2, -2, -2, -2, -2, -2 };
    char want[40] = "";
    FILE *f;
    int i;

    if (sceUserServiceGetLoginUserIdList(login) != 0) login[0] = login[1] = login[2] = login[3] = -1;
    if (sceUserServiceGetInitialUser(&init) != 0) init = -1;
    if (sceUserServiceGetForegroundUser(&fore) != 0) fore = -1;
    if ((f = fopen("/data/anypad/bind_user", "r"))) {
        if (fgets(want, sizeof want, f)) {
            for (i = 0; want[i]; i++) if (want[i] == '\n' || want[i] == '\r' || want[i] == ' ') want[i] = 0;
            if (strncmp(want, "0x", 2) == 0) memmove(want, want + 2, strlen(want + 2) + 1);
        }
        fclose(f);
    }

    memcpy(seen, login, sizeof login);
    seen[4] = init;
    seen[5] = fore;
    if (memcmp(seen, logged, sizeof seen) != 0) {                /* log it when it changes */
        memcpy(logged, seen, sizeof seen);
        log_line("users: initial %#x, foreground %#x, signed in %#x %#x %#x %#x%s%s", (unsigned)init,
                 (unsigned)fore, (unsigned)login[0], (unsigned)login[1], (unsigned)login[2],
                 (unsigned)login[3], want[0] ? ", bind_user=" : "", want);
    }
    return vpad_choose_user(login, init, fore, want);
}

/* System-service credentials: MBus refuses a plain payload. Every step is
 * checked and read back; on any failure the original credentials are put
 * back and virtual pads stay off. */
static uint64_t g_orig_authid;
static uint8_t g_orig_caps[16];
static int g_have_orig;
static int g_raised;

/* Sets credentials and reads them back. Returns 0 if they did not stick. */
static int set_creds(uint64_t authid, const uint8_t caps[16])
{
    pid_t me = getpid();
    uint8_t now_caps[16];

    return kernel_set_ucred_authid(me, authid) == 0 && kernel_set_ucred_caps(me, caps) == 0 &&
           kernel_get_ucred_authid(me) == authid && kernel_get_ucred_caps(me, now_caps) == 0 &&
           memcmp(now_caps, caps, 16) == 0;
}

static int elevate(void)
{
    static const uint8_t caps[16] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    pid_t me = getpid();

    if (!g_have_orig) {
        g_orig_authid = kernel_get_ucred_authid(me);
        if (!g_orig_authid || kernel_get_ucred_caps(me, g_orig_caps) != 0) {
            log_line("vpad: cannot read our credentials");
            return 0;
        }
        g_have_orig = 1;
    }
    if (!set_creds(0x3800000000010003ull, caps)) {
        log_line("vpad: credentials not raised; putting the old ones back");
        set_creds(g_orig_authid, g_orig_caps);
        g_raised = 0;
        return 0;
    }
    g_raised = 1;
    return 1;
}

int vpad_creds_restore(void)
{
    if (!g_have_orig || !g_raised) return 1;
    if (!set_creds(g_orig_authid, g_orig_caps)) {
        log_line("vpad: could not put the original credentials back");
        return 0;
    }
    g_raised = 0;
    log_line("vpad: original credentials restored");
    return 1;
}

int vpad_creds_raise(void)
{
    return g_raised || elevate();
}

int vpad_init(void)
{
    static const char *const mbus[] = {
        "/system/common/lib/libSceMbus.sprx", "libSceMbus.sprx",
    };
    void *lib = NULL;
    int32_t r;
    size_t i;

    if (!elevate()) return 0;
    if ((r = sceUserServiceInitialize(NULL)) != 0)
        log_line("vpad: sceUserServiceInitialize %#x (already initialised is fine)", (unsigned)r);

    /* libScePad imports from libSceMbus: load it before any scePad call,
     * or scePadInit can fault on an unresolved import. */
    for (i = 0; i < sizeof mbus / sizeof mbus[0] && !lib; i++)
        lib = dlopen(mbus[i], RTLD_NOW | RTLD_GLOBAL);
    if (lib) g_bind = (mbus_bind_fn)dlsym(lib, "sceMbusBindDeviceWithUserId");
    if (!g_bind) {
        log_line("vpad: libSceMbus not available, refusing to touch libScePad");
        return 0;
    }
    /* scePadInit first. scePadSetProcessPrivilege before it answers
     * 0x80920005, "not initialised" (found on the first run on a console,
     * firmware 10.01). The
     * privilege call is not checked as a precondition: the virtual device
     * calls report their own errors. */
    if ((r = scePadInit()) != 0) {
        log_line("vpad: scePadInit failed: %#x", (unsigned)r);
        return 0;
    }
    if ((r = scePadSetProcessPrivilege(1)) != 0)
        log_line("vpad: scePadSetProcessPrivilege answered %#x, carrying on", (unsigned)r);
    g_user = main_user();
    log_line("vpad: main user %#x", (unsigned)g_user);
    g_ready = 1;
    return 1;
}

/* ---- virtual pads ----------------------------------------------------------- */

int vpad_add(int slot)
{
    if (!g_ready || slot < 0 || slot >= HOST_MAX_PADS) return 0;
    if (g_slot[slot].st != VP_FREE && g_slot[slot].st != VP_FAILED) return 0;
    memset(&g_slot[slot], 0, sizeof g_slot[slot]);
    g_slot[slot].st = VP_QUEUED;
    g_slot[slot].handle = -1;
    return 1;
}

static void start_identification(int slot)
{
    int32_t param[8];
    int32_t r;
    int i;

    if (g_user == -1) g_user = main_user();     /* signed in since */
    if (g_user == -1) {
        log_line("slot %d: nobody signed in, no virtual pad", slot);
        g_slot[slot].st = VP_FAILED;
        return;
    }
    if (!vpad_creds_raise()) {
        g_slot[slot].st = VP_FAILED;
        return;
    }
    g_log_fd = log_source_open();
    if (g_log_fd < 0) {
        g_slot[slot].st = VP_FAILED;
        return;
    }
    log_source_read();                  /* what came before is not ours */
    g_matches = 0;
    g_match_dev = 0;

    /* The parameter block as the PS5 accepts it: size 32, user 1,
     * the rest marked so that a call writing into it shows in the log. */
    for (i = 0; i < 8; i++) param[i] = (int32_t)0xDEADBEEF;
    param[0] = (int32_t)sizeof param;
    param[1] = 1;
    r = scePadVirtualDeviceAddDevice(param, VIRTUAL_DEVICE_DUALSENSE);
    log_line("slot %d: AddDevice %#x, block %08x %08x %08x %08x %08x %08x", slot, (unsigned)r,
             (unsigned)param[2], (unsigned)param[3], (unsigned)param[4], (unsigned)param[5],
             (unsigned)param[6], (unsigned)param[7]);
    g_pending = slot;
    g_t_add = now_ms();
    g_slot[slot].st = VP_PENDING;
}

static void finish_identification(long now)
{
    vslot *v = &g_slot[g_pending];
    int slot = g_pending;
    int32_t r;

    if (g_matches == 0 && now - g_t_add < T_IDENTIFY) return;
    if (g_matches == 1 && now - g_t_match < T_AMBIGUITY && now - g_t_add < T_IDENTIFY) return;

    g_pending = -1;
    log_source_close();
    if (g_matches != 1) {
        /* Unknown or ambiguous: nothing is bound, inserted into or deleted. */
        log_line("slot %d: virtual pad not identified (%d candidates), left alone", slot, g_matches);
        v->st = VP_FAILED;
        return;
    }
    v->dev = g_match_dev;
    v->handle = (int32_t)(g_match_dev & 0xFFFFFFFFu);
    if (v->remove_when_found) {
        scePadVirtualDeviceDeleteDevice(v->handle);
        log_line("slot %d: device %#llx removed: its pad left meanwhile", slot,
                 (unsigned long long)v->dev);
        memset(v, 0, sizeof *v);
        return;
    }
    r = g_bind(v->dev, g_user);
    log_line("slot %d: device %#llx bound to user %#x: %#x", slot, (unsigned long long)v->dev,
             (unsigned)g_user, (unsigned)r);
    if (r != 0) {
        scePadVirtualDeviceDeleteDevice(v->handle);     /* ours, identified */
        v->st = VP_FAILED;
        return;
    }
    v->st = VP_READY;
}

void vpad_poll(long now)
{
    int i;

    if (!g_ready) return;
    ps_tick(now);
    if (g_pending >= 0) {
        log_source_read();
        finish_identification(now);
        return;
    }
    for (i = 0; i < HOST_MAX_PADS; i++)
        if (g_slot[i].st == VP_QUEUED) {
            start_identification(i);
            return;
        }
}

int vpad_live(int slot)
{
    return slot >= 0 && slot < HOST_MAX_PADS && g_slot[slot].st == VP_READY;
}

/* One frame to the virtual pad: the pad's state, with PS added while the menu
 * is "holding" it. */
static void send_frame(int slot, const pad_state *st)
{
    ScePadData d;
    struct timespec ts;
    int i;

    if (!vpad_live(slot)) return;
    memset(&d, 0, sizeof d);
    d.buttons = st->buttons | (g_slot[slot].ps_until ? PAD_PS : 0);
    d.lx = st->lx;
    d.ly = st->ly;
    d.rx = st->rx;
    d.ry = st->ry;
    d.l2 = st->l2;
    d.r2 = st->r2;
    d.orientation[3] = 1.0f;
    if (st->has_motion) {
        for (i = 0; i < 3; i++) {
            d.acceleration[i] = st->accel[i] / ACCEL_PER_G;
            d.angularVelocity[i] = st->gyro[i] / GYRO_PER_DEG_S * DEG_TO_RAD;
        }
    } else {
        d.acceleration[1] = -1.0f;      /* at rest, held level */
    }
    if (st->has_touch) {
        for (i = 0; i < 2; i++) {
            ScePadTouch *t = &d.touchData.touch[d.touchData.touchNum];
            if (!st->touch[i].down) continue;
            t->x = st->touch[i].x;
            t->y = st->touch[i].y;
            t->id = (uint8_t)i;
            d.touchData.touchNum++;
        }
    }
    d.connected = 1;
    d.connectedCount = 1;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    d.timestamp = (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;

    if (scePadVirtualDeviceInsertData(g_slot[slot].handle, &d) != 0) {
        /* Stop feeding a device that refuses: it may not be ours any more. */
        log_line("slot %d: the virtual pad refused input, stopped feeding it", slot);
        g_slot[slot].st = VP_FAILED;
    }
}

void vpad_update(int slot, const pad_state *st)
{
    if (!vpad_live(slot)) return;
    g_slot[slot].last = *st;
    send_frame(slot, st);
}

/* Holds PS on a virtual pad for `ms`, as pressing the PS button on the pad
 * would: the console hands the pad over to its user, or opens its menu. */
int vpad_press_ps(int slot, int ms)
{
    if (!vpad_live(slot) || ms <= 0) return 0;
    g_slot[slot].ps_until = now_ms() + ms;
    g_slot[slot].ps_active = 1;
    log_line("slot %d: PS pressed from the menu (%d ms)", slot, ms);
    send_frame(slot, &g_slot[slot].last);
    return 1;
}

/* Keeps the PS frames going (the pad may be silent) and ends the press. */
static void ps_tick(long now)
{
    int i;

    for (i = 0; i < HOST_MAX_PADS; i++) {
        vslot *v = &g_slot[i];
        if (!v->ps_active || v->st != VP_READY) continue;
        if (v->ps_until && now >= v->ps_until) v->ps_until = 0;       /* released */
        send_frame(i, &v->last);
        if (!v->ps_until) v->ps_active = 0;
    }
}

/* Removes only what this instance created and identified. */
void vpad_remove(int slot)
{
    vslot *v;

    if (slot < 0 || slot >= HOST_MAX_PADS) return;
    v = &g_slot[slot];
    switch (v->st) {
    case VP_READY:
        scePadVirtualDeviceDeleteDevice(v->handle);
        log_line("slot %d: virtual DualSense removed", slot);
        memset(v, 0, sizeof *v);
        break;
    case VP_PENDING:
        v->remove_when_found = 1;       /* finish_identification deletes it */
        break;
    default:
        memset(v, 0, sizeof *v);
        break;
    }
}

static int32_t g_phys_err;
static int32_t g_phys_handle = -1;
static int g_phys_ours;                 /* we opened it, so we close it */
static long g_phys_retry_at;
static int32_t g_open_logged = 1;
static long g_user_retry_at;

int32_t vpad_physical_error(void) { return g_phys_err; }

/* The handle for the user's pad in this process. A payload has none until
 * it opens the pad, which is what any application does (the system's own
 * UI and the running game each hold the pad too). A refusal is logged and
 * tried again at most every 5 seconds. */
static int32_t physical_handle(void)
{
    int32_t h;

    if (g_phys_handle >= 0) return g_phys_handle;
    h = scePadGetHandle(g_user, 0, 0);
    if (h < 0 && now_ms() >= g_phys_retry_at) {
        g_phys_retry_at = now_ms() + 5000;
        h = scePadOpen(g_user, 0, 0, NULL);
        if (h != g_open_logged) {                   /* only when the answer changes */
            log_line("vpad: scePadOpen of the user's pad (user %#x): %#x", (unsigned)g_user, (unsigned)h);
            g_open_logged = h;
        }
        if (h >= 0) g_phys_ours = 1;
        /* The user may sign in or out meanwhile: look again. */
        if (h == (int32_t)0x809B0081) g_user = main_user();
    }
    if (h >= 0) g_phys_handle = h;
    return h;
}

void vpad_close_physical(void)
{
    if (g_phys_handle >= 0 && g_phys_ours) scePadClose(g_phys_handle);
    g_phys_handle = -1;
    g_phys_ours = 0;
}

int vpad_read_physical(uint32_t *buttons)
{
    ScePadData d;
    int32_t h, r;

    /* Only reads the user's own pad, through the handle libScePad gives for
     * it: never opens, takes over or disconnects it. */
    if (!g_ready) {
        g_phys_err = (int32_t)0xFFFF0001;               /* not ready */
        return 0;
    }
    if (g_user == -1) {                 /* nobody was signed in; look again now and then */
        if (now_ms() >= g_user_retry_at) {
            g_user_retry_at = now_ms() + 5000;
            g_user = main_user();
        }
        if (g_user == -1) {
            g_phys_err = (int32_t)0xFFFF0004;           /* nobody signed in */
            return 0;
        }
    }
    h = physical_handle();
    if (h < 0) {
        g_phys_err = h;
        return 0;
    }
    r = scePadReadState(h, &d);
    if (r != 0) {
        g_phys_err = r;
        if (g_phys_ours) scePadClose(h);
        g_phys_handle = -1;             /* start again from the handle */
        g_phys_ours = 0;
        return 0;
    }
    if (!d.connected) {
        g_phys_err = (int32_t)0xFFFF0002;               /* no pad connected */
        return 0;
    }
    g_phys_err = 0;
    /* The system takes the pad's input while it has focus: that reads as the
     * 'intercepted' bit, and then the buttons are not ours to use. */
    if (d.buttons & 0x80000000u) {
        g_phys_err = (int32_t)0xFFFF0003;               /* intercepted */
        return 0;
    }
    *buttons = d.buttons;
    return 1;
}
