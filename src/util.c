#include "util.h"

#include <time.h>

long (*ph_clock_hook)(void);

long now_ms(void)
{
    struct timespec ts;

    if (ph_clock_hook) return ph_clock_hook();
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
