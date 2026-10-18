/* AnyPad PS5 -- Bluetooth game controllers on the PlayStation 5.
 *
 * Pads from other consoles and brands connect to the console's own
 * Bluetooth chip, beside the DualSense, and appear to games as DualSense
 * controllers of the main user.
 *
 *   hci_usb.c   the Bluetooth chip, shared with the system's driver
 *   host.c      pairing, reconnection, L2CAP, SDP and HID (Classic)
 *   le.c        the same over Bluetooth LE
 *   profiles.c  each controller's reports
 *   ps5_vpad.c  the virtual DualSense each pad drives
 *   web.c       the menu: a page on port 8095
 *   (no console hotkey: a plain payload cannot read the DualSense; the menu opens from the media row)
 *
 * ALPHA: see PRUEBAS.md for what has been tried on a console.
 *
 * Stability comes first. It only touches what it created: its own Bluetooth
 * links, its own virtual pads, its own files under /data/anypad. When
 * something it depends on fails, it says so (log, notification, the menu)
 * and stays out of the way instead of retrying forever.
 *
 * The menu is started first, before anything that can fail, so that it is
 * there to explain a failure. It is a web page: from a phone or a PC at the
 * address the start-up notification gives, or on the console itself by
 * holding a button combination on the DualSense (L2 + R2 + OPTIONS for 1.5
 * seconds unless /data/anypad/config.ini says otherwise).
 *
 * Stopping: the menu's button, or create /data/anypad/stop. It disconnects
 * its pads, removes its virtual pads, puts back the controller setting it
 * changed, and exits. It does the same by itself when the console starts
 * going to rest mode (read from SceSystemStateMgrInfo), so nothing of it is left half-way across a sleep.
 * Pairing: the menu's button, or create /data/anypad/pair.
 */
#include "config.h"
#include "hci_usb.h"
#include "host.h"
#include "lock.h"
#include "log.h"
#include "netinfo.h"
#include "ps5_power.h"
#include "ps5_ui.h"
#include "ps5_apps.h"
#include "ps5_sysinfo.h"
#include "ps5_vpad.h"
#include "usb_pad.h"
#include "netpad.h"
#include "util.h"
#include "version.h"
#include "web.h"

#if defined(TARGET_PS4)
#define ANYPAD_PLATFORM "PS4"
#define ANYPAD_PAD_NAME "DualShock 4"
#else
#define ANYPAD_PLATFORM "PS5"
#define ANYPAD_PAD_NAME "DualSense"
#endif

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STATE_DIR    "/data/anypad"
#define LOG_PATH     STATE_DIR "/anypad.log"
#define DB_PATH      STATE_DIR "/pads.db"
#define LOCK_PATH    STATE_DIR "/anypad.lock"
#define CONFIG_PATH  STATE_DIR "/config.ini"
#define PSCOMBO_PATH STATE_DIR "/ps_combos.txt"   /* «Configure PS»: kept, per model */
#define PAIR_FLAG    STATE_DIR "/pair"
#define STOP_FLAG    STATE_DIR "/stop"
#define NO_ICON_FLAG STATE_DIR "/no_icon"          /* do not put AnyPad in the media row */
#define VPAD_TEST_FLAG STATE_DIR "/vpad_test"      /* one idle virtual pad on slot 0, to test them alone */
#define RM_ICON_FLAG STATE_DIR "/remove_icon"      /* take it out, once */
#define PAIR_SECONDS 60

#define OPEN_TRIES      5       /* opening Bluetooth: 5 tries, 3 s apart */
#define RAISED_TRIES    2       /* ... the first two with raised credentials */

/* Findable in the binary: strings AnyPad-PS5-*.elf | grep anypad-version */
/* The commit the ELF was built from, written by tools/build-elf-docker.sh. */
#if defined(__has_include)
#if __has_include("build_id.h")
#include "build_id.h"
#endif
#endif
#ifndef ANYPAD_BUILD_ID
#define ANYPAD_BUILD_ID "unknown"
#endif

static const char g_version_tag[] __attribute__((used)) = "anypad-version " ANYPAD_VERSION;

static volatile sig_atomic_t g_stop;
static volatile int g_web_stop;         /* the menu's stop button */
static volatile int g_retry;            /* the menu's retry button */
static volatile int g_bt_state = WEB_BT_TRYING;
static char g_bt_reason[200];

static host_t *g_host;                  /* NULL until Bluetooth is up */
static web_t *g_web;
static usb_pad_mgr_t *g_usb_pads;
static int g_vpad_test;                 /* VPAD_TEST_FLAG was there */
static int g_vpad_ok;                   /* virtual pads are possible */
static int g_bt_attempt;                /* Bluetooth tries made in this round */
static long g_bt_next;                  /* when the next one is due */
static int g_bt_plain_creds;            /* the chip answered only without raised credentials */

static void on_usb_feed(int slot, const pad_state *st)
{
    if (vpad_live(slot)) {
        vpad_update(slot, st);
    }
}

static void on_usb_attach(int slot, const char *name)
{
    log_line("usb_pad: attached %s to slot %d", name, slot);
    vpad_add(slot);
}

static void on_usb_detach(int slot)
{
    log_line("usb_pad: detached slot %d", slot);
    vpad_remove(slot);
}

/* Stopping must never hang: while this process lives it holds reads on the
 * Bluetooth chip, and the console's DualSense cannot reconnect. If the
 * goodbye takes too long, leave at once; the kernel frees the transfers. */
#define T_STOP_MAX_S 10
static void on_stop_timeout(int sig)
{
    (void)sig;
    unlink(DB_PATH);                    /* nothing of this session is kept */
    _exit(0);
}

/* The console's language decides the texts: Spanish, or English for any other. */
static int g_spanish = 1;
static int g_lang_code = 3;         /* the console's language: the menu page follows it */
#define TR(es, en) (g_spanish ? (es) : (en))

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ---- virtual pads follow the controllers ---------------------------------- */

/* «Configure PS»: each slot's model, to find its combination. While the
 * user picks a new one on the menu, the old one is not applied. */
static struct { uint16_t vid, pid; long capture_until; } g_pad_id[HOST_MAX_PADS];

static void on_connect(void *ud, int slot, const pad_info *info)
{
    (void)ud;
    if (slot >= 0 && slot < HOST_MAX_PADS) {
        g_pad_id[slot].vid = info->vid;
        g_pad_id[slot].pid = info->pid;
        g_pad_id[slot].capture_until = 0;
    }
    if (vpad_add(slot)) notify(TR("AnyPad PS5: %s conectado", "AnyPad PS5: %s connected"), info->profile);
    else notify(TR("AnyPad PS5: %s conectado, pero sin mando virtual (ver el log)", "AnyPad PS5: %s connected, but no virtual controller (see the log)"), info->profile);
}

static void on_state(void *ud, int slot, const pad_state *st)
{
    uint32_t combo = 0;

    (void)ud;
    if (slot >= 0 && slot < HOST_MAX_PADS && now_ms() >= g_pad_id[slot].capture_until)
        combo = pscombo_get(g_pad_id[slot].vid, g_pad_id[slot].pid);
    if (combo && (st->buttons & combo) == combo) {
        pad_state c = *st;
        c.buttons = pscombo_apply(combo, c.buttons);
        vpad_update(slot, &c);
        return;
    }
    vpad_update(slot, st);
}

static void web_ps_combo_get(int slot, char *out, size_t max)
{
    out[0] = 0;
    if (slot < 0 || slot >= HOST_MAX_PADS) return;
    config_combo_keys(pscombo_get(g_pad_id[slot].vid, g_pad_id[slot].pid), out, max);
}

static int web_ps_combo_set(int slot, const char *text)
{
    uint32_t mask = 0;
    char shown[100];

    if (slot < 0 || slot >= HOST_MAX_PADS || !g_pad_id[slot].vid) return 0;
    if (strcmp(text, "capture") == 0) {
        g_pad_id[slot].capture_until = now_ms() + 60000;
        return 1;
    }
    if (strcmp(text, "none") != 0 && !config_parse_keys(text, &mask)) return 0;
    if (!pscombo_set(PSCOMBO_PATH, g_pad_id[slot].vid, g_pad_id[slot].pid, mask)) return 0;
    g_pad_id[slot].capture_until = 0;
    config_combo_text(mask, shown, sizeof shown);
    log_line("slot %d: PS combination for %04x:%04x %s%s", slot, g_pad_id[slot].vid, g_pad_id[slot].pid,
             mask ? "set to " : "removed", mask ? shown : "");
    return 1;
}

/* The menu's PS button: the same as holding PS on that pad. */
#define PS_HOLD_MS 700
static int web_press_ps(int slot)
{
    return g_vpad_ok && vpad_press_ps(slot, PS_HOLD_MS);
}

static void on_disconnect(void *ud, int slot)
{
    (void)ud;
    vpad_remove(slot);
    notify(TR("AnyPad PS5: mando %d desconectado", "AnyPad PS5: controller %d disconnected"), slot + 1);
}

/* ---- Bluetooth, one try at a time ------------------------------------------ */

/* The host calls this whenever it waits for the chip, so the menu answers
 * even while a Bluetooth try is in progress (it used to freeze for ~5 s each
 * time, and the browser's pile-up of connections got "busy"). */
static void on_idle(void *ud)
{
    (void)ud;
    web_poll(g_web);
}

static void bt_begin(void)
{
    g_bt_attempt = 0;
    g_bt_next = 0;
    g_bt_reason[0] = '\0';
#if defined(TARGET_PS4)
    /* The PS4's Bluetooth is not a USB chip AnyPad can drive; trying would
     * open the USB pads' nodes instead. */
    g_bt_state = WEB_BT_FAILED;
    snprintf(g_bt_reason, sizeof g_bt_reason, "Bluetooth aun no disponible en PS4: usa los mandos por cable.");
    return;
#endif
    g_bt_state = WEB_BT_TRYING;
    if (g_vpad_ok) vpad_creds_raise();
}

/* One try. The controller may answer only to a process that does not carry
 * raised credentials (not known), so the first
 * tries are made as they are and the rest with the original ones. Which
 * worked is logged. Virtual pads raise them again when they need to. */
static void bt_try(long now)
{
    host_events ev = { on_connect, on_state, on_disconnect, NULL, on_idle };
    hci_t hci;
    int attempt = ++g_bt_attempt;

    if (attempt == RAISED_TRIES + 1 && g_vpad_ok) {
#if !defined(TARGET_PS4)
        log_line("Bluetooth silent with raised credentials; trying with the original ones");
        if (vpad_creds_restore()) g_bt_plain_creds = 1;
#endif
    }
    if (hci_usb_open(&hci)) {
        g_host = host_open(hci, DB_PATH, &ev);
        if (g_host) {
            g_bt_state = WEB_BT_OK;
            log_line("Bluetooth ready after %d tr%s%s", attempt, attempt == 1 ? "y" : "ies",
                     g_bt_plain_creds ? ", answering only with the original credentials" : "");
            if (hci.ops->missing_reads && hci.ops->missing_reads(hci.ctx) > 0)
                notify(TR("AnyPad PS5: otro programa usa el Bluetooth (otro payload sigue en marcha?). Paralo desde su menu: AnyPad se recupera solo",
                          "AnyPad PS5: another program is using Bluetooth (is another payload still running?). Stop it from its menu: AnyPad recovers by itself"));
            if (host_paired_count(g_host))      /* opened again in this session (after rest mode) */
                notify(TR("AnyPad PS5: Bluetooth listo (%d mandos de esta sesion)", "AnyPad PS5: Bluetooth ready (%d controllers of this session)"), host_paired_count(g_host));
            else
                notify(TR("AnyPad PS5: Bluetooth listo. Empareja los mandos desde el menu", "AnyPad PS5: Bluetooth ready. Pair the controllers from the menu"));
            return;
        }
        hci.ops->close(hci.ctx);
    }
    log_line("Bluetooth not ready (try %d of %d)", attempt, OPEN_TRIES);
    if (attempt >= OPEN_TRIES) {
        g_bt_state = WEB_BT_FAILED;
        snprintf(g_bt_reason, sizeof g_bt_reason,
                 "El chip Bluetooth no responde (%d intentos, con y sin credenciales elevadas). "
                 "Detalle en el log, lineas 'diag'.%s", attempt,
                 hci_usb_chip_vendor() == 0x1286
                     ? " Este chip (Marvell/NXP) puede quedarse colgado tras un reinicio: apaga la consola del todo y vuelve a encenderla."
                     : "");
        log_line("Bluetooth unavailable: %s", g_bt_reason);
        notify(TR("AnyPad PS5: el Bluetooth no responde.\nSigo activo: abre el menu para ver el motivo", "AnyPad PS5: Bluetooth does not answer.\nStill running: open the menu to see why"));
        return;
    }
    g_bt_next = now + 3000;
}

static void bt_lost(const char *why)
{
    log_line("Bluetooth lost: %s", why);
    host_close(g_host);                 /* drops every pad and its virtual pad */
    g_host = NULL;
    g_bt_state = WEB_BT_FAILED;
    snprintf(g_bt_reason, sizeof g_bt_reason, "Se perdio el Bluetooth (%s). Pulsa Reintentar.", why);
    notify(TR("AnyPad " ANYPAD_PLATFORM ": se perdio el Bluetooth.\nPulsa Reintentar en el menu", "AnyPad " ANYPAD_PLATFORM ": Bluetooth was lost.\nPress Retry in the menu"));
}

/* ---- flags ---------------------------------------------------------------- */

static void check_flags(long now)
{
    static long last_second;

    if (now / 1000 == last_second) return;
    last_second = now / 1000;
    if (access(STOP_FLAG, F_OK) == 0) {
        unlink(STOP_FLAG);
        g_stop = 1;
    }
    if (g_host && access(PAIR_FLAG, F_OK) == 0) {
        unlink(PAIR_FLAG);
        host_pair(g_host, PAIR_SECONDS);
        notify(TR("AnyPad " ANYPAD_PLATFORM ": emparejando %d s. Pon el mando en modo emparejamiento", "AnyPad " ANYPAD_PLATFORM ": pairing for %d s. Put the controller in pairing mode"), PAIR_SECONDS);
    }
}

int main(void)
{
    web_cfg wc;
    char ip[16], url[64];
    const char *why = "stop requested";
    long last_power = 0, last_net = 0;

    mkdir(STATE_DIR, 0755);
    log_open(LOG_PATH);
    log_line("AnyPad " ANYPAD_PLATFORM " %s (build %s)", ANYPAD_VERSION, ANYPAD_BUILD_ID);
    (void)g_version_tag;
    g_lang_code = ui_console_language();
    g_spanish = ui_language_is_spanish(g_lang_code);    /* notifications: Spanish, or English */

    if (!lock_take(LOCK_PATH)) {
        log_line("another instance is running");
        notify(TR("AnyPad " ANYPAD_PLATFORM ": ya esta en marcha", "AnyPad " ANYPAD_PLATFORM ": already running"));
        log_close();
        return 1;
    }
    unlink(STOP_FLAG);                  /* an old request is not for us */
    /* One session per run: pads are paired anew each time AnyPad starts, and
     * nothing is kept once it stops or the console restarts. The store lives
     * only while this run does (so pads still reconnect after rest mode). */
    if (unlink(DB_PATH) == 0) log_line("session: pads of an earlier run discarded");
    log_line("session: pads are paired for this run only");
    syscall(SYS_thr_set_name, -1, "anypad");    /* how others find us */
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);           /* a browser closing early */


    /* The menu first: it is what explains everything that follows. */
    memset(&wc, 0, sizeof wc);
    wc.host = &g_host;
    wc.stop = &g_web_stop;
    wc.retry = &g_retry;
    wc.bt_state = &g_bt_state;
    wc.bt_reason = g_bt_reason;
    wc.log_path = LOG_PATH;
    wc.version = ANYPAD_VERSION;
    wc.chip = hci_usb_chip_name();
    wc.lang = ui_language_tag(g_lang_code);
    wc.press_ps = web_press_ps;
    wc.ps_combo_get = web_ps_combo_get;
    wc.ps_combo_set = web_ps_combo_set;
    log_line("ps combos: %d model(s) with a PS combination", pscombo_load(PSCOMBO_PATH));
    g_web = web_start(&wc, WEB_PORT);

    local_ip(ip);
    snprintf(url, sizeof url, "http://%s:%d/", ip, WEB_PORT);
    if (g_web)
        notify(TR("AnyPad " ANYPAD_PLATFORM " %s\nMenu: %s\nAl emparejar un mando, elige Juego rapido / Invitado cuando la consola lo pregunte", "AnyPad " ANYPAD_PLATFORM " %s\nMenu: %s\nWhen you pair a controller, choose Quick play / Guest when the console asks"), ANYPAD_VERSION, url);
    else
        notify(TR("AnyPad " ANYPAD_PLATFORM " %s\nNo se pudo abrir la pagina del menu: puerto %d ocupado. Si otro payload de mandos sigue en marcha, paralo desde su menu", "AnyPad " ANYPAD_PLATFORM " %s\nCould not open the menu page: port %d in use. If another controller payload is still running, stop it from its menu"),
               ANYPAD_VERSION, WEB_PORT);

    sysinfo_log();

    /* The entry in the media row that opens the menu in the browser. Done
     * before the credentials are raised for the pads, as Payload Manager does
     * it with its own; /data/anypad/no_icon skips it, remove_icon undoes it. */
    if (access(RM_ICON_FLAG, F_OK) == 0) {
        apps_remove_launcher();
        unlink(RM_ICON_FLAG);
    } else if (access(NO_ICON_FLAG, F_OK) != 0) {
        apps_install_launcher();
    }

    /* Without virtual pads there is nothing to connect controllers to:
     * Bluetooth is not touched at all, and the menu says why. */
    g_vpad_ok = vpad_init();
    if (!g_vpad_ok) {
        g_bt_state = WEB_BT_FAILED;
        snprintf(g_bt_reason, sizeof g_bt_reason,
                 "No se pueden crear mandos virtuales en este sistema (ver el log). No se ha tocado el Bluetooth.");
        notify(TR("AnyPad " ANYPAD_PLATFORM ": no se pueden crear mandos virtuales.\nVer el menu o el log. El Bluetooth no se ha tocado", "AnyPad " ANYPAD_PLATFORM ": cannot create virtual controllers.\nSee the menu or the log. Bluetooth was not touched"));
    } else {
        bt_begin();
        g_usb_pads = usb_pad_init(on_usb_feed, on_usb_attach, on_usb_detach);
        /* Controllers over the network (a phone/PC as the pad, the way
         * Control4Free does it): its own port and thread, feeding the same
         * virtual pads. Gives wireless pads without the console's own radio. */
        if (netpad_start(NETPAD_PORT))
            notify(TR("AnyPad " ANYPAD_PLATFORM ": mando por red en http://%s:%d/ (abre esa pagina en el movil)",
                      "AnyPad " ANYPAD_PLATFORM ": network controller at http://%s:%d/ (open that page on your phone)"),
                   ip, NETPAD_PORT);
        if (access(VPAD_TEST_FLAG, F_OK) == 0) {
            log_line("vpad test: one idle virtual pad on slot 0");
            vpad_add(0);
            g_vpad_test = 1;
        }
    }

    while (!g_stop && !g_web_stop) {
        long now;

        if (g_host) host_poll(g_host, 4);
        else usleep(4000);
        now = now_ms();
        web_poll(g_web);
        if (g_vpad_ok) {
            vpad_poll(now);
            if (g_usb_pads) usb_pad_poll(g_usb_pads, now);
            if (g_vpad_test && vpad_live(0)) {
                pad_state idle;

                pad_state_reset(&idle);
                vpad_update(0, &idle);
            }
        }

        /* The page follows the network, as Payload Manager's does: every 5 s the
         * console's address is looked at and the page is served again if the
         * address changed, the network was lost, or the listener died (pulling
         * the cable or changing the DNS leaves it dead with the process alive). */
        if (now - last_net >= 5000) {
            char nip[16];
            int changed;

            last_net = now;
            local_ip(nip);
            changed = strcmp(nip, ip) != 0;
            if (changed || !g_web) {
                if (changed) log_line("network: address changed from %s to %s", ip, nip);
                if (g_web) web_stop(g_web);
                g_web = web_start(&wc, WEB_PORT);
                snprintf(ip, sizeof ip, "%s", nip);
                if (g_web && strcmp(nip, "127.0.0.1") != 0 && changed)
                    notify(TR("AnyPad " ANYPAD_PLATFORM ": pagina de nuevo en http://%s:%d/", "AnyPad " ANYPAD_PLATFORM ": page back at http://%s:%d/"), nip, WEB_PORT);
            }
        }

        /* Rest mode on its way: leave cleanly before it, not after. */
        if (now - last_power >= 200) {
            int ps = power_state();
            last_power = now;
            if (ps == POWER_GOING_TO_REST || ps == POWER_IN_REST) {
                why = "rest mode";
                notify(TR("AnyPad " ANYPAD_PLATFORM ": parado por el modo reposo.\nLanzalo de nuevo al despertar", "AnyPad " ANYPAD_PLATFORM ": stopped by rest mode.\nRun it again after waking up"));
                break;
            }
        }

        /* The Bluetooth: tries, loss, and the menu's retry button. */
        if (g_bt_state == WEB_BT_TRYING && now >= g_bt_next) bt_try(now);
        if (g_host && host_transport_lost(g_host)) bt_lost("sin respuesta del chip");
        if (g_retry) {
            g_retry = 0;
            if (g_vpad_ok && g_bt_state == WEB_BT_FAILED) {
                log_line("retrying Bluetooth at the user's request");
                bt_begin();
            } else {
                log_line("retry ignored (virtual pads not available, or not failed)");
            }
        }

        check_flags(now);
    }
    if (g_web_stop) why = "stop from the menu";
    else if (g_stop && strcmp(why, "stop requested") == 0) why = "stop requested (flag or signal)";

    log_line("stopping: %s", why);
    signal(SIGALRM, on_stop_timeout);
    alarm(T_STOP_MAX_S);
    if (g_usb_pads) {
        usb_pad_close(g_usb_pads);
        g_usb_pads = NULL;
    }
    netpad_stop();                      /* drops network clients before their pads go */
    web_stop(g_web);
    host_close(g_host);                 /* our links; removes our virtual pads */
    g_host = NULL;
    unlink(DB_PATH);                    /* the session ends: nothing is kept */
    log_line("stopping: Bluetooth released, pads of this session discarded");
    if (g_vpad_ok) {
        int i;

        for (i = 0; i < HOST_MAX_PADS; i++) vpad_remove(i);    /* the test and USB ones too */
        vpad_creds_restore();           /* leave the credentials as found */
    }
    lock_release();
    log_line("stopped");
    log_close();
    notify(TR("AnyPad " ANYPAD_PLATFORM ": parado", "AnyPad " ANYPAD_PLATFORM ": stopped"));
    return 0;
}
