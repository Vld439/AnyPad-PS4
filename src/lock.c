#include "lock.h"
#include "log.h"

#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fd = -1;
static char g_path[256];

long lock_boot_time(void)
{
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    struct timeval tv;
    size_t len = sizeof tv;

    if (sysctl(mib, 2, &tv, &len, NULL, 0) != 0) return 0;
    return (long)tv.tv_sec;
}

/* Is the lock at `path` held by a copy that is really running? */
static int lock_is_live(const char *path, long boot)
{
    char buf[64];
    struct stat st;
    long pid = 0, lock_boot = 0;
    int fd = open(path, O_RDONLY), fields;
    ssize_t n;

    if (fd < 0) return 0;
    n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    fields = sscanf(buf, "%ld %ld", &pid, &lock_boot);
    if (fields < 1 || pid <= 0 || pid == (long)getpid()) return 0;

    if (fields == 2) {
        if (boot && lock_boot != boot) return 0;        /* from another boot */
    } else if (boot && stat(path, &st) == 0 && (long)st.st_mtime < boot) {
        return 0;                       /* an older lock (id only), written before this boot */
    }
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

int lock_take(const char *path)
{
    char buf[64];
    long boot = lock_boot_time();
    int attempt;

    snprintf(g_path, sizeof g_path, "%s", path);
    for (attempt = 0; attempt < 2; attempt++) {
        int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
        if (fd >= 0) {
            int n = snprintf(buf, sizeof buf, "%d %ld\n", (int)getpid(), boot);
            if (write(fd, buf, (size_t)n) != n) {
                close(fd);
                unlink(path);
                return 0;
            }
            g_fd = fd;
            return 1;
        }
        if (errno != EEXIST) return 0;
        if (lock_is_live(path, boot)) return 0;         /* another copy runs */
        log_line("stale lock, taking it over");
        unlink(path);
    }
    return 0;
}

void lock_release(void)
{
    if (g_fd < 0) return;
    close(g_fd);
    unlink(g_path);
    g_fd = -1;
}
