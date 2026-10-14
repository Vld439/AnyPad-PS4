#include "ps5_sysinfo.h"
#include "log.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <ps5/kernel.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Processes that are the system's own and not worth listing. */
static int is_system_name(const char *n)
{
    static const char *const skip[] = { "kernel", "idle", "init", "swapper", "intr", "pagedaemon",
                                        "vmdaemon", "bufdaemon", "syncer", "usb", "geom" };
    size_t i;

    if (strncmp(n, "Sce", 3) == 0 || strncmp(n, "sce", 3) == 0) return 1;
    for (i = 0; i < sizeof skip / sizeof skip[0]; i++)
        if (strncmp(n, skip[i], strlen(skip[i])) == 0) return 1;
    return 0;
}

void sysinfo_log(void)
{
    uint32_t fw = kernel_get_fw_version();
    int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
    size_t len = 0, i, n;
    char *buf;
    char line[400];
    int used = 0;

    /* The firmware is stored as binary-coded decimal: 0x10010000 is 10.01. */
    log_line("console: firmware %x.%02x (raw %#x), pid %d", (unsigned)(fw >> 24) & 0xFF,
             (unsigned)(fw >> 16) & 0xFF, (unsigned)fw, (int)getpid());

    if (sysctl(mib, 3, NULL, &len, NULL, 0) != 0 || len == 0) return;
    len += len / 8;                             /* processes come and go between the two calls */
    if (!(buf = malloc(len))) return;
    if (sysctl(mib, 3, buf, &len, NULL, 0) == 0) {
        used = snprintf(line, sizeof line, "console: other processes:");
        for (i = 0; i + sizeof(struct kinfo_proc) <= len && used < (int)sizeof line - 40; i += n) {
            struct kinfo_proc kp;
            char name[sizeof kp.ki_comm + 1];

            memcpy(&kp, buf + i, sizeof kp);
            n = kp.ki_structsize > 0 ? (size_t)kp.ki_structsize : sizeof kp;
            memcpy(name, kp.ki_comm, sizeof kp.ki_comm);
            name[sizeof kp.ki_comm] = '\0';
            if (!name[0] || is_system_name(name) || kp.ki_pid == getpid()) continue;
            if (strstr(line, name) && strstr(line, name)[strlen(name)] == ',') continue;   /* once */
            used += snprintf(line + used, sizeof line - (size_t)used, " %s,", name);
        }
        log_line("%s", line);
    }
    free(buf);
}
