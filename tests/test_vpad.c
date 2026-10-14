/* vpad_init against stand-ins for the console's libraries, to check the
 * order of the libScePad calls and what is fatal. Compiled on the host from
 * the console source itself. */
#include "../src/ps5_vpad.h"
#include "../src/util.h"


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* What the stand-ins were asked, in order. */
static char g_calls[64];
static int g_inited, g_priv_result_after_init, g_init_result;
static uint64_t g_authid = 0x4800000000000001ull;
static uint8_t g_caps[16];

static long g_now = 100000;
static long fake_clock(void) { return g_now; }

static void note(char c) { size_t n = strlen(g_calls); if (n < sizeof g_calls - 1) g_calls[n] = c; }

#define SCE_PAD_ERROR_NOT_INITIALIZED ((int32_t)0x80920005)

int32_t sceUserServiceInitialize(void *p) { (void)p; note('u'); return 0; }
static int32_t g_init_user = 0x10000000, g_fore_user = 0x10000000;
static int32_t g_login[4] = { 0x10000000, -1, -1, -1 };
int32_t sceUserServiceGetInitialUser(int32_t *u) { *u = g_init_user; return 0; }
int32_t sceUserServiceGetForegroundUser(int32_t *u) { *u = g_fore_user; return 0; }
int32_t sceUserServiceGetLoginUserIdList(int32_t list[4]) { memcpy(list, g_login, sizeof g_login); return 0; }
int32_t scePadInit(void) { note('I'); if (g_init_result == 0) g_inited = 1; return g_init_result; }
int32_t scePadSetProcessPrivilege(int32_t v)
{
    (void)v;
    note('P');
    return g_inited ? g_priv_result_after_init : SCE_PAD_ERROR_NOT_INITIALIZED;
}
static int g_pad_opened, g_open_result = 7, g_closed;
static unsigned g_pad_buttons;
static int g_pad_connected = 1;
int32_t scePadGetHandle(int32_t u, int32_t t, int32_t i)
{
    (void)u; (void)t; (void)i;
    return g_pad_opened ? 7 : (int32_t)0x80920008;      /* NO_HANDLE until opened */
}
static int32_t g_open_user;
int32_t scePadOpen(int32_t u, int32_t t, int32_t i, const void *p)
{
    int k, signed_in = 0;
    (void)t; (void)i; (void)p;
    g_open_user = u;
    for (k = 0; k < 4; k++) if (g_login[k] == u) signed_in = 1;
    if (!signed_in) return (int32_t)0x809B0081;         /* SCE_DEVICE_SERVICE_ERROR_USER_NOT_LOGIN */
    if (g_open_result >= 0) g_pad_opened = 1;
    return g_open_result;
}
int32_t scePadClose(int32_t h) { (void)h; g_closed++; g_pad_opened = 0; return 0; }
int32_t scePadReadState(int32_t h, void *d)
{
    /* ScePadData: buttons at 0, connected at offset 76 (after the touch data) */
    unsigned char *p = d;
    if (h != 7) return (int32_t)0x80920003;
    memset(p, 0, 120);
    memcpy(p, &g_pad_buttons, 4);
    p[76] = (unsigned char)g_pad_connected;
    return 0;
}
int32_t scePadVirtualDeviceAddDevice(void *p, int32_t t) { (void)p; (void)t; return 0; }
int32_t scePadVirtualDeviceInsertData(int32_t h, const void *d) { (void)h; (void)d; return 0; }
int32_t scePadVirtualDeviceDeleteDevice(int32_t h) { (void)h; return 0; }
uint64_t kernel_get_ucred_authid(pid_t pid) { (void)pid; return g_authid; }
int32_t kernel_set_ucred_authid(pid_t pid, uint64_t a) { (void)pid; g_authid = a; return 0; }
int32_t kernel_get_ucred_caps(pid_t pid, uint8_t c[16]) { (void)pid; memcpy(c, g_caps, 16); return 0; }
int32_t kernel_set_ucred_caps(pid_t pid, const uint8_t c[16]) { (void)pid; memcpy(g_caps, c, 16); return 0; }
int sceKernelSendNotificationRequest(int a, void *r, size_t n, int b) { (void)a; (void)r; (void)n; (void)b; return 0; }

static int32_t fake_bind(uint64_t d, int32_t u) { (void)d; (void)u; return 0; }
void *dlopen(const char *path, int mode) { (void)path; (void)mode; return (void *)1; }
void *dlsym(void *h, const char *name) { (void)h; (void)name; return (void *)fake_bind; }

static void reset(int init_result, int priv_after_init)
{
    memset(g_calls, 0, sizeof g_calls);
    g_inited = 0;
    g_init_result = init_result;
    g_priv_result_after_init = priv_after_init;
}

int main(void)
{
    printf("vpad_init: libScePad is initialised before its privilege is set\n");
    reset(0, 0);
    CHECK(vpad_init() == 1);
    CHECK(strstr(g_calls, "IP") != NULL);               /* init, then privilege */
    CHECK(strchr(g_calls, 'P') > strchr(g_calls, 'I'));

    printf("a privilege answer that is not 0 is a warning, as in the references\n");
    reset(0, SCE_PAD_ERROR_NOT_INITIALIZED);
    CHECK(vpad_init() == 1);

    printf("a failing scePadInit is fatal, and the privilege is never set\n");
    reset((int32_t)0x80920001, 0);
    CHECK(vpad_init() == 0);
    CHECK(strchr(g_calls, 'P') == NULL);

    printf("the original credentials can be put back and raised again\n");
    reset(0, 0);
    CHECK(vpad_init() == 1);
    CHECK(g_authid == 0x3800000000010003ull);               /* raised */
    CHECK(vpad_creds_restore() == 1);
    CHECK(g_authid == 0x4800000000000001ull && g_caps[0] == 0x00);   /* as found at the start */
    CHECK(vpad_creds_restore() == 1);                       /* twice is harmless */
    CHECK(vpad_creds_raise() == 1);
    CHECK(g_authid == 0x3800000000010003ull);

    printf("the user's pad: no handle, so it is opened the normal way; read; closed at the end\n");
    {
        uint32_t b = 0;
        ph_clock_hook = fake_clock;
        reset(0, 0);
        CHECK(vpad_init() == 1);
        g_pad_buttons = 0x00104400u;                        /* L1 + R1 + touchpad */
        CHECK(vpad_read_physical(&b) == 1);
        CHECK(g_pad_opened == 1 && b == 0x00104400u);
        CHECK(vpad_physical_error() == 0);
        g_pad_buttons = 0x80000000u | 0x400;                /* the system has the input */
        CHECK(vpad_read_physical(&b) == 0 && vpad_physical_error() == (int32_t)0xFFFF0003);
        g_pad_buttons = 0;
        g_pad_connected = 0;
        CHECK(vpad_read_physical(&b) == 0 && vpad_physical_error() == (int32_t)0xFFFF0002);
        g_pad_connected = 1;
        vpad_close_physical();
        CHECK(g_closed == 1 && g_pad_opened == 0);
        g_pad_opened = 0;
        g_open_result = (int32_t)0x80920004;                /* opening is refused */
        CHECK(vpad_read_physical(&b) == 0);                 /* tried again at most every 5 s: */
        CHECK(vpad_physical_error() == (int32_t)0x80920008);
        g_now += 6000;
        CHECK(vpad_read_physical(&b) == 0);
        CHECK(vpad_physical_error() == (int32_t)0x80920004);
        g_open_result = 7;
    }

    printf("the user given the pads is one who is signed in\n");
    {
        uint32_t b = 0;
        ph_clock_hook = fake_clock;

        /* the initial user is signed in: preferred over the foreground one */
        g_init_user = 0x2000; g_fore_user = 0x3000;
        g_login[0] = 0x3000; g_login[1] = 0x2000; g_login[2] = g_login[3] = -1;
        g_pad_opened = 0; g_closed = 0; g_now += 10000;
        reset(0, 0);
        CHECK(vpad_init() == 1);
        vpad_close_physical();
        g_pad_opened = 0;
        g_now += 10000;
        CHECK(vpad_read_physical(&b) == 1 && g_open_user == 0x2000);

        /* the initial user is not signed in; the foreground one is */
        vpad_close_physical();
        g_pad_opened = 0;
        g_login[0] = 0x3000; g_login[1] = g_login[2] = g_login[3] = -1;
        reset(0, 0);
        g_now += 10000;
        CHECK(vpad_init() == 1);                   /* picks again */
        vpad_close_physical();
        g_pad_opened = 0;
        g_now += 10000;
        CHECK(vpad_read_physical(&b) == 1 && g_open_user == 0x3000);

        /* neither: the first one that is signed in */
        vpad_close_physical();
        g_pad_opened = 0;
        g_init_user = 0x2000; g_fore_user = 0x2500;
        g_login[0] = 0x4000; g_login[1] = g_login[2] = g_login[3] = -1;
        reset(0, 0);
        g_now += 10000;
        CHECK(vpad_init() == 1);
        vpad_close_physical();
        g_pad_opened = 0;
        g_now += 10000;
        CHECK(vpad_read_physical(&b) == 1 && g_open_user == 0x4000);

        /* nobody signed in: no pad can be read, and no user is made up */
        vpad_close_physical();
        g_pad_opened = 0;
        g_login[0] = g_login[1] = g_login[2] = g_login[3] = -1;
        reset(0, 0);
        g_now += 10000;
        CHECK(vpad_init() == 1);
        g_now += 10000;
        CHECK(vpad_read_physical(&b) == 0 && g_pad_opened == 0);

        /* USER_NOT_LOGIN on open: the user is looked for again, at the next try */
        g_init_user = 0x2000;
        g_login[0] = 0x2000;
        g_now += 10000;
        CHECK(vpad_read_physical(&b) == 1 && g_open_user == 0x2000);
        vpad_close_physical();
        g_pad_opened = 0;
        g_init_user = 0x10000000; g_fore_user = 0x10000000; g_login[0] = 0x10000000;
    }

    printf("vpad_choose_user: default order, and the bind_user override\n");
    {
        int32_t two[4] = { 0x1111, 0x2222, -1, -1 };
        int32_t one[4] = { 0x1111, -1, -1, -1 };
        int32_t none[4] = { -1, -1, -1, -1 };

        CHECK(vpad_choose_user(two, 0x1111, 0x2222, NULL) == 0x1111);       /* the owner */
        CHECK(vpad_choose_user(two, 0x1111, 0x2222, "") == 0x1111);
        CHECK(vpad_choose_user(two, 0x1111, 0x2222, "other") == 0x2222);    /* someone else */
        CHECK(vpad_choose_user(one, 0x1111, 0x1111, "other") == 0x1111);    /* nobody else: default */
        CHECK(vpad_choose_user(two, 0x1111, 0x1111, "2222") == 0x2222);     /* an id */
        CHECK(vpad_choose_user(two, 0x1111, 0x1111, "9999") == 0x1111);     /* not signed in: default */
        CHECK(vpad_choose_user(two, 0x1111, 0x1111, "zz") == 0x1111);       /* not an id: default */
        CHECK(vpad_choose_user(none, 0x1111, 0x1111, "other") == -1);       /* nobody signed in */
    }

    printf(g_fail ? "%d check(s) failed\n" : "all vpad checks passed\n", g_fail);
    return g_fail != 0;
}
