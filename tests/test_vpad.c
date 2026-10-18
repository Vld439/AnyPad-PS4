/* vpad_init and the slots against stand-ins for the console's libraries, to
 * check the order of the libScePad calls, what is fatal, and how slots are
 * taken (the network clients rely on that to find a free one). Compiled on
 * the host from the console source itself. */
#include "../src/ps4_vpad.h"


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* What the stand-ins were asked, in order. */
static char g_calls[64];
static int g_inited, g_priv_result_after_init, g_init_result, g_adds;
static uint64_t g_authid = 0x4800000000000001ull;
static uint8_t g_caps[16];

static void note(char c) { size_t n = strlen(g_calls); if (n < sizeof g_calls - 1) g_calls[n] = c; }

#define SCE_PAD_ERROR_NOT_INITIALIZED ((int32_t)0x80920005)

int32_t sceUserServiceInitialize(void *p) { (void)p; note('u'); return 0; }
static int32_t g_login[4] = { 0x10000000, -1, -1, -1 };
int32_t sceUserServiceGetInitialUser(int32_t *u) { *u = 0x10000000; return 0; }
int32_t sceUserServiceGetForegroundUser(int32_t *u) { *u = 0x10000000; return 0; }
int32_t sceUserServiceGetLoginUserIdList(int32_t list[4]) { memcpy(list, g_login, sizeof g_login); return 0; }
int32_t scePadInit(void) { note('I'); if (g_init_result == 0) g_inited = 1; return g_init_result; }
int32_t scePadSetProcessPrivilege(int32_t v)
{
    (void)v;
    note('P');
    return g_inited ? g_priv_result_after_init : SCE_PAD_ERROR_NOT_INITIALIZED;
}
int32_t scePadVirtualDeviceAddDevice(void *p, int32_t t) { (void)p; (void)t; g_adds++; return 0; }
int32_t scePadVirtualDeviceInsertData(int32_t h, const void *d) { (void)h; (void)d; return 0; }
int32_t scePadVirtualDeviceDeleteDevice(int32_t h) { (void)h; return 0; }
uint64_t kernel_get_ucred_authid(pid_t pid) { (void)pid; return g_authid; }
int32_t kernel_set_ucred_authid(pid_t pid, uint64_t a) { (void)pid; g_authid = a; return 0; }
int32_t kernel_get_ucred_caps(pid_t pid, uint8_t c[16]) { (void)pid; memcpy(c, g_caps, 16); return 0; }
int32_t kernel_set_ucred_caps(pid_t pid, const uint8_t c[16]) { (void)pid; memcpy(g_caps, c, 16); return 0; }
int sceKernelSendNotificationRequest(int a, void *r, size_t n, int b) { (void)a; (void)r; (void)n; (void)b; return 0; }

static void reset(int init_result, int priv_after_init)
{
    memset(g_calls, 0, sizeof g_calls);
    g_inited = 0;
    g_init_result = init_result;
    g_priv_result_after_init = priv_after_init;
}

int main(void)
{
    char label[40];
    pad_state st;
    int s;

    printf("slots: nothing is taken before vpad_init\n");
    CHECK(vpad_add(0) == 0);

    printf("vpad_init: libScePad is initialised before its privilege is set\n");
    reset(0, 0);
    CHECK(vpad_init() == 1);
    CHECK(strstr(g_calls, "IP") != NULL);               /* init, then privilege */
    CHECK(strchr(g_calls, 'P') > strchr(g_calls, 'I'));

    printf("a privilege answer that is not 0 is a warning, as in the references\n");
    reset(0, SCE_PAD_ERROR_NOT_INITIALIZED);
    CHECK(vpad_init() == 1);

    printf("a failing scePadInit is fatal (also with the original credentials), and the privilege is never set\n");
    reset((int32_t)0x80920001, 0);
    CHECK(vpad_init() == 0);
    CHECK(strchr(g_calls, 'P') == NULL);
    CHECK(strstr(g_calls, "II") != NULL);               /* tried again as the payload */

    printf("the original credentials can be put back and raised again\n");
    reset(0, 0);
    CHECK(vpad_init() == 1);
    CHECK(g_authid == 0x3800000000010003ull);               /* raised */
    CHECK(vpad_creds_restore() == 1);
    CHECK(g_authid == 0x4800000000000001ull && g_caps[0] == 0x00);   /* as found at the start */
    CHECK(vpad_creds_restore() == 1);                       /* twice is harmless */
    CHECK(vpad_creds_raise() == 1);
    CHECK(g_authid == 0x3800000000010003ull);

    printf("slots: a slot asked for is taken until removed; the lowest free one is found\n");
    CHECK(vpad_add(0) == 1);
    CHECK(vpad_add(0) == 0);                                /* taken */
    vpad_label(0, "network");
    for (s = 0; s < MAX_PADS && !vpad_add(s); s++) {}        /* as netpad looks for one */
    CHECK(s == 1);
    CHECK(vpad_add(MAX_PADS) == 0 && vpad_add(-1) == 0);
    CHECK(vpad_get(0, label, sizeof label, &st) == 0);       /* asked for, not live yet */
    CHECK(vpad_press_ps(0, 700) == 0);                      /* nothing to press on */
    vpad_remove(0);
    vpad_remove(1);
    CHECK(vpad_add(0) == 1);                                /* free again */
    vpad_remove(0);

    printf("slots: nobody signed in, no device is asked for, and the slot can be asked again\n");
    g_login[0] = -1;
    g_adds = 0;
    CHECK(vpad_add(2) == 1);
    vpad_poll(1000);
    CHECK(g_adds == 0);
    CHECK(vpad_live(2) == 0);
    CHECK(vpad_add(2) == 1);                                /* failed: reusable */
    vpad_remove(2);
    g_login[0] = 0x10000000;

    printf(g_fail ? "%d check(s) failed\n" : "all vpad checks passed\n", g_fail);
    return g_fail != 0;
}
