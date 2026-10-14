#include "log.h"
#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The log is kept under LOG_MAX bytes: past it, it becomes path.1 (the
 * one before is dropped) and a new one starts. */
#define LOG_MAX (1L << 20)
#define LIVE_LINES 600
#define LIVE_LEN   200

static FILE *g_log;
static char g_path[256];
static char g_live[LIVE_LINES][LIVE_LEN];
static unsigned long g_seq;

int log_open(const char *path)
{
    if (!path) return 1;
    snprintf(g_path, sizeof g_path, "%s", path);
    g_log = fopen(path, "a");
    return g_log != NULL;
}

void log_close(void)
{
    if (g_log) fclose(g_log);
    g_log = NULL;
}

static void rotate(void)
{
    char old[sizeof g_path + 2];

    if (!g_log || ftell(g_log) < LOG_MAX) return;
    fclose(g_log);
    snprintf(old, sizeof old, "%s.1", g_path);
    remove(old);
    rename(g_path, old);
    g_log = fopen(g_path, "a");
}

void log_line(const char *fmt, ...)
{
    FILE *out;
    va_list ap;
    char *line = g_live[g_seq % LIVE_LINES];
    int k;

    rotate();
    out = g_log ? g_log : stderr;
    k = snprintf(line, LIVE_LEN, "[%8ld] ", now_ms());
    va_start(ap, fmt);
    if (k > 0 && k < LIVE_LEN) vsnprintf(line + k, (size_t)(LIVE_LEN - k), fmt, ap);
    va_end(ap);
    g_seq++;
    fprintf(out, "[%8ld] ", now_ms());
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
    fflush(out);
}

unsigned long log_seq(void) { return g_seq; }

unsigned long log_since(unsigned long from, char *out, size_t n)
{
    size_t used = 0;

    if (n == 0) return from;
    out[0] = 0;
    if (g_seq > LIVE_LINES && from < g_seq - LIVE_LINES) from = g_seq - LIVE_LINES;
    for (; from < g_seq; from++) {
        const char *l = g_live[from % LIVE_LINES];
        size_t len = strlen(l);
        if (used + len + 2 > n) break;
        memcpy(out + used, l, len);
        used += len;
        out[used++] = '\n';
        out[used] = 0;
    }
    return from;
}
