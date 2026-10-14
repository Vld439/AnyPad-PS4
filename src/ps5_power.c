#include "ps5_power.h"
#include "log.h"

#include <stdint.h>

int sceKernelOpenEventFlag(intptr_t *flag, const char *name);
int sceKernelPollEventFlag(intptr_t flag, uint64_t bits, unsigned int mode, uint64_t *result);
int sceKernelCloseEventFlag(intptr_t flag);

#define WAIT_OR         2
#define STATE_SUSPENDING  300
#define STATE_STANDBY     500
#define STATE_WORKING    1000

static intptr_t g_flag = -1;

int power_state(void)
{
    uint64_t bits = 0;

    if (g_flag < 0 && sceKernelOpenEventFlag(&g_flag, "SceSystemStateMgrInfo") < 0) {
        g_flag = -1;
        return POWER_UNKNOWN;
    }
    if (sceKernelPollEventFlag(g_flag, UINT64_MAX, WAIT_OR, &bits) < 0) {
        sceKernelCloseEventFlag(g_flag);
        g_flag = -1;
        return POWER_UNKNOWN;
    }
    switch ((unsigned)(bits & 0xFFFF)) {
    case STATE_WORKING:     return POWER_WORKING;
    case STATE_SUSPENDING:  return POWER_GOING_TO_REST;
    case STATE_STANDBY:     return POWER_IN_REST;
    default:                return POWER_UNKNOWN;
    }
}
