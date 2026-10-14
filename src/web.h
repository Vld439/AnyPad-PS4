/* AnyPad PS5's web page: status, pairing, forgetting pads, stopping, the log.
 *
 * Served from the main loop without threads or blocking: at most WEB_CLIENTS
 * connections, requests of at most 8 KB, three seconds each.
 *
 * There is no PIN, by the user's choice: anyone on the local network can
 * open the page, pair, forget pads and stop AnyPad PS5. Nothing else.
 */
#ifndef PH_WEB_H
#define PH_WEB_H

#include "host.h"

#include <stddef.h>

#define WEB_PORT 8095

typedef struct web web_t;

enum { WEB_BT_TRYING, WEB_BT_OK, WEB_BT_FAILED };

typedef struct {
    host_t **host;              /* the host; NULL until Bluetooth is up */
    volatile int *stop;         /* set to 1 to ask AnyPad PS5 to stop */
    volatile int *retry;        /* set to 1 to try the Bluetooth again */
    volatile int *bt_state;     /* a WEB_BT_* */
    const char *bt_reason;      /* why not, when it failed (plain text) */
    const char *log_path;       /* tail served at /api/log */
    const char *version;
    const char *lang;           /* "es" or "en": the page's texts. NULL: "es" */
    const char *chip;           /* the Bluetooth chip, for the page ("MediaTek (0e8d:3603)"). NULL: unknown */
    const char *combo_text;     /* the console hotkey, in words */
    /* The user's own pad: returns 1 and its buttons if readable, else 0 and
     * the reason in *err. NULL: not available. */
    int (*physical)(uint32_t *buttons, int *err);
    /* Presses PS on pad `slot` (0-based). Returns 1 if done. NULL: not available. */
    int (*press_ps)(int slot);
    /* «Configure PS»: the combination standing for PS on pad `slot`'s model,
     * as "l1+l2+options" ("" if none). NULL: not available. */
    void (*ps_combo_get)(int slot, char *out, size_t max);
    /* Sets it ("l1+l2+options"), removes it ("none"), or suspends it while
     * the user picks a new one ("capture"). Returns 1 if done. */
    int (*ps_combo_set)(int slot, const char *text);
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
