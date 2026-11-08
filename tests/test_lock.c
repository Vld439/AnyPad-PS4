/* The instance lock: live, dead, from another boot, and in the old format. */
#include "../src/lock.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define LOCK "build/test_lock.lock"

static void write_lock(const char *text)
{
    FILE *f = fopen(LOCK, "w");
    fputs(text, f);
    fclose(f);
}

static pid_t dead_pid(void)
{
    pid_t p = fork();
    if (p == 0) _exit(0);
    waitpid(p, NULL, 0);
    return p;
}

int main(void)
{
    char text[64];
    long boot = lock_boot_time();
    struct timeval old[2];

    printf("the instance lock\n");
    CHECK(boot > 1000000000L);                      /* the boot time is readable */
    unlink(LOCK);

    CHECK(lock_take(LOCK));                         /* free: taken */
    {
        FILE *f = fopen(LOCK, "r");
        long pid = 0, b = 0;
        CHECK(f && fscanf(f, "%ld %ld", &pid, &b) == 2 && pid == (long)getpid() && b == boot);
        if (f) fclose(f);
    }
    lock_release();
    CHECK(access(LOCK, F_OK) != 0);                 /* released: gone */

    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot);
    write_lock(text);
    CHECK(!lock_take(LOCK));                        /* a live owner of this boot: refused */
    CHECK(access(LOCK, F_OK) == 0);                 /* and its lock is left alone */

    snprintf(text, sizeof text, "%d %ld\n", (int)dead_pid(), boot);
    write_lock(text);
    CHECK(!lock_take(LOCK));                        /* its id is dead, but it said "here" a moment ago */
    old[0].tv_sec = old[1].tv_sec = time(NULL) - 10; /* a crashed copy stops saying it */
    old[0].tv_usec = old[1].tv_usec = 0;
    utimes(LOCK, old);
    CHECK(lock_take(LOCK));                         /* its process is gone and it is not fresh: stale */
    lock_release();

    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot - 100);
    write_lock(text);
    CHECK(lock_take(LOCK));                         /* a live id, but from another boot: stale */
    lock_release();

    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot + 1);
    write_lock(text);
    CHECK(!lock_take(LOCK));                        /* a second off between the clocks: the same boot */

    {
        struct sockaddr_in sin;
        int on = 1, fd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&sin, 0, sizeof sin);
        sin.sin_family = AF_INET;
        sin.sin_port = htons(8095);
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        CHECK(bind(fd, (struct sockaddr *)&sin, sizeof sin) == 0 && listen(fd, 4) == 0);
        snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot + 3600);
        write_lock(text);
        CHECK(!lock_take(LOCK));                    /* the wall clock was set, its menu up: live */
        close(fd);
        unlink(LOCK);
    }

    snprintf(text, sizeof text, "%d\n", (int)getppid());
    write_lock(text);                               /* old format, written just now */
    CHECK(!lock_take(LOCK));                        /* live: refused */
    old[0].tv_sec = old[1].tv_sec = boot - 3600;    /* ... and the same file from before the boot */
    old[0].tv_usec = old[1].tv_usec = 0;
    utimes(LOCK, old);
    CHECK(lock_take(LOCK));                         /* stale by its age */
    lock_release();

    write_lock("garbage\n");
    CHECK(lock_take(LOCK));                         /* unreadable: stale */
    lock_release();
    write_lock("");
    CHECK(lock_take(LOCK));                         /* empty: stale */
    lock_release();

    printf(g_fail ? "%d check(s) failed\n" : "all lock checks passed\n", g_fail);
    return g_fail != 0;
}
