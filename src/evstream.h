/* HCI events as they come over USB.
 *
 * Events arrive on an interrupt endpoint in pieces of at most one packet (16
 * bytes on the PS5's chip); a longer event (an extended inquiry result is 255
 * bytes) is several pieces, and a piece may also hold the end of one event and
 * the start of the next. This puts them back together. A piece lost on the way
 * (taken by another reader of the same endpoint) would leave a half event
 * waiting forever, so one that has waited too long is dropped. */
#ifndef ANYPAD_EVSTREAM_H
#define ANYPAD_EVSTREAM_H

#include <stdint.h>

#define EVSTREAM_MAX     258        /* 2-byte header + 255 */
#define EVSTREAM_TIMEOUT 100        /* ms a half event may wait for its rest */

typedef struct {
    uint8_t buf[EVSTREAM_MAX + 16];
    int len;
    long t_last;                    /* when the last piece arrived */
    unsigned dropped;               /* half events given up on */
} evstream;

typedef void (*evstream_emit)(void *ctx, const uint8_t *event, int len);

void evstream_init(evstream *s);

/* Feeds one piece; calls emit for every event completed by it. */
void evstream_feed(evstream *s, const uint8_t *piece, int len, long now,
                   evstream_emit emit, void *ctx);

#endif
