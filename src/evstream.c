#include "evstream.h"

#include <string.h>

void evstream_init(evstream *s)
{
    memset(s, 0, sizeof *s);
}

void evstream_feed(evstream *s, const uint8_t *piece, int len, long now,
                   evstream_emit emit, void *ctx)
{
    if (len <= 0) return;
    if (s->len && now - s->t_last > EVSTREAM_TIMEOUT) {
        s->len = 0;                 /* the rest of that event is not coming */
        s->dropped++;
    }
    s->t_last = now;

    while (len > 0) {
        int room = (int)sizeof s->buf - s->len, take = len < room ? len : room;

        memcpy(s->buf + s->len, piece, (size_t)take);
        s->len += take;
        piece += take;
        len -= take;

        for (;;) {
            int need;
            if (s->len < 2) break;
            need = 2 + s->buf[1];
            if (s->len < need) break;
            emit(ctx, s->buf, need);
            memmove(s->buf, s->buf + need, (size_t)(s->len - need));
            s->len -= need;
        }
    }
}
