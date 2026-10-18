/* AnyPad's web page: status, the controllers, the network controller's
 * address, pressing PS, stopping, the log.
 *
 * Served from the main loop without threads or blocking: at most WEB_CLIENTS
 * connections, requests of at most 8 KB, three seconds each.
 *
 * There is no PIN, by the user's choice: anyone on the local network can
 * open the page, press PS on a pad and stop AnyPad. Nothing else.
 */
#ifndef PH_WEB_H
#define PH_WEB_H

#include "pad.h"

#include <stddef.h>

#define WEB_PORT 8095

typedef struct web web_t;

typedef struct {
    volatile int *stop;         /* set to 1 to ask AnyPad to stop */
    const char *log_path;       /* tail served at /api/log */
    const char *version;
    const char *lang;           /* the console's language tag: the page's texts. NULL: "es" */
    const char *net_url;        /* the network controller's page ("http://ip:8096/"). NULL: none */
    /* Clients driving a pad over the network. NULL: none. */
    int (*net_clients)(void);
    /* A live pad's label and state (slot 0-based). Returns 0 if none. */
    int (*pad_get)(int slot, char *label, size_t max, pad_state *st);
    /* Presses PS on pad `slot` (0-based). Returns 1 if done. NULL: not available. */
    int (*press_ps)(int slot);
} web_cfg;

/* Listens on `port` on every interface. Returns NULL if it cannot. */
web_t *web_start(const web_cfg *cfg, int port);
void   web_poll(web_t *w);

/* Is the page still being served? The listening socket must still listen and
 * a connection to this machine's own address must be taken. The console can
 * leave a listener dead when its network is torn down or reconfigured (the
 * cable pulled, the DNS changed), with the process still running. */
int    web_alive(const web_t *w, int port);
void   web_stop(web_t *w);

#endif
