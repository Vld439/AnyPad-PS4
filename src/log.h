/* One log line per call: to a file when opened with a path, else stderr. */
#ifndef PH_LOG_H
#define PH_LOG_H

#include <stddef.h>

int  log_open(const char *path);
void log_close(void);
void log_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Dialog mode: the last lines are also kept in memory, numbered from 0, so
 * a client can ask for the lines written since the last one it saw. */
unsigned long log_seq(void);    /* number of the next line to be written */
/* Copies the lines numbered `from` on (the oldest kept, if `from` is older),
 * one per row, into out (at most n bytes, always terminated). Returns the
 * number of the line after the last one copied. */
unsigned long log_since(unsigned long from, char *out, size_t n);

#endif
