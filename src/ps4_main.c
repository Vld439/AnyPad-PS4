/* AnyPad PS4 -- other controllers on the PlayStation 4.
 *
 * Pads from other consoles and brands appear to games as DualShock 4
 * controllers. Two ways in:
 *
 *   usb_pad.c   pads on the console's USB ports (Xbox 360, Xbox One, HID)
 *   netpad.c    a phone or PC on the local network reading a pad (paired
 *               to it by Bluetooth, or plugged into it) and sending it here
 *   profiles.c  each controller's reports
 *   ps4_vpad.c  the virtual DualShock 4 each pad drives
 *   web.c       the menu: a page on port 8095
 *
 * The console's own Bluetooth radio and Sony's own pads are the system's:
 * AnyPad does not touch them.
 *
 * Stability comes first. It only touches what it created: its own virtual
 * pads, the USB interfaces it claimed, its own files under /data/anypad.
 * When something it depends on fails, it says so (log, notification, the
 * menu) and stays out of the way instead of retrying forever.
 *
 * The menu is started first, before anything that can fail, so that it is
 * there to explain a failure. It is a web page: from a phone or a PC at the
 * address the start-up notification gives, or on the console from the
 * media row.
 *
 * Stopping: the menu's button, or create /data/anypad/stop. It releases its
 * pads, removes its virtual pads, puts back its credentials, and exits. It
 * does the same by itself when the console starts going to rest mode (read
 * from SceSystemStateMgrInfo), so nothing of it is left half-way across a
 * sleep.
 */
#include "lock.h"
#include "log.h"
#include "netinfo.h"
#include "netpad.h"
#include "ps4_apps.h"
#include "ps4_power.h"
#include "ps4_sysinfo.h"
#include "ps4_ui.h"
#include "ps4_vpad.h"
#include "usb_pad.h"
#include "util.h"
#include "version.h"
#include "web.h"

#include <sys/stat.h>
#include <sys/syscall.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STATE_DIR    "/data/anypad"
#define LOG_PATH     STATE_DIR "/anypad.log"
#define LOCK_PATH    STATE_DIR "/anypad.lock"
#define STOP_FLAG    STATE_DIR "/stop"
#define NO_ICON_FLAG STATE_DIR "/no_icon"          /* do not put AnyPad in the media row */
#define VPAD_TEST_FLAG STATE_DIR "/vpad_test"      /* one idle virtual pad on slot 0, to test them alone */
#define RM_ICON_FLAG STATE_DIR "/remove_icon"      /* take it out, once */

/* Findable in the binary: strings AnyPad-PS4-*.elf | grep anypad-version */
/* The commit the ELF was built from, written by tools/build-elf-ps4-docker.sh. */
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

static web_t *g_web;
static usb_pad_mgr_t *g_usb_pads;
static int g_vpad_test;                 /* VPAD_TEST_FLAG was there */
static int g_vpad_ok;                   /* virtual pads are possible */
static char g_net_url[64];              /* the network controller's page, for the menu */
static int g_net_up;                    /* the network controller is being served */

/* The console's language decides the texts: Spanish, or English for any other. */
static int g_spanish = 1;
#define TR(es, en) (g_spanish ? (es) : (en))

/* ---- virtual pads follow the controllers ---------------------------------- */

static void on_usb_feed(int slot, const pad_state *st)
{
    if (vpad_live(slot)) vpad_update(slot, st);
}

static void on_usb_attach(int slot, const char *name)
{
    log_line("usb_pad: attached %s to slot %d", name, slot);
    if (vpad_add(slot)) vpad_label(slot, name);
}

static void on_usb_detach(int slot)
{
    log_line("usb_pad: detached slot %d", slot);
    vpad_remove(slot);
}

/* The menu's PS button: the same as holding PS on that pad. */
#define PS_HOLD_MS 700
static int web_press_ps(int slot)
{
    return g_vpad_ok && vpad_press_ps(slot, PS_HOLD_MS);
}

/* Stopping must never hang: if the goodbye takes too long, leave at once;
 * the kernel frees what is left. */
#define T_STOP_MAX_S 10
static void on_stop_timeout(int sig)
{
    (void)sig;
    _exit(0);
}

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void check_flags(long now)
{
    static long last_second;

    if (now / 1000 == last_second) return;
    last_second = now / 1000;
    if (access(STOP_FLAG, F_OK) == 0) {
        unlink(STOP_FLAG);
        g_stop = 1;
    }
}

int main(void)
{
    web_cfg wc;
    char ip[16], url[64];
    const char *why = "stop requested";
    long last_power = 0, last_net = 0;
    int lang;

    mkdir(STATE_DIR, 0755);
    log_open(LOG_PATH);
    log_line("AnyPad PS4 %s (build %s)", ANYPAD_VERSION, ANYPAD_BUILD_ID);
    (void)g_version_tag;
    lang = ui_console_language();
    g_spanish = ui_language_is_spanish(lang);           /* notifications: Spanish, or English */

    if (!lock_take(LOCK_PATH)) {
        log_line("another instance is running");
        notify(TR("AnyPad PS4: ya esta en marcha", "AnyPad PS4: already running"));
        log_close();
        return 1;
    }
    unlink(STOP_FLAG);                  /* an old request is not for us */
    syscall(SYS_thr_set_name, -1, "anypad");    /* how others find us */
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);           /* a browser closing early */

    /* The menu first: it is what explains everything that follows. */
    local_ip(ip);
    snprintf(g_net_url, sizeof g_net_url, "http://%s:%d/", ip, NETPAD_PORT);
    memset(&wc, 0, sizeof wc);
    wc.stop = &g_web_stop;
    wc.log_path = LOG_PATH;
    wc.version = ANYPAD_VERSION;
    wc.lang = ui_language_tag(lang);
    wc.net_url = g_net_url;
    wc.net_clients = netpad_clients;
    wc.pad_get = vpad_get;
    wc.press_ps = web_press_ps;
    g_web = web_start(&wc, WEB_PORT);

    snprintf(url, sizeof url, "http://%s:%d/", ip, WEB_PORT);
    if (g_web)
        notify(TR("AnyPad PS4 %s\nMenu: %s\nCuando la consola pregunte quien usa un mando, elige tu usuario", "AnyPad PS4 %s\nMenu: %s\nWhen the console asks who is using a controller, choose your user"), ANYPAD_VERSION, url);
    else
        notify(TR("AnyPad PS4 %s\nNo se pudo abrir la pagina del menu: puerto %d ocupado. Si otro payload de mandos sigue en marcha, paralo desde su menu", "AnyPad PS4 %s\nCould not open the menu page: port %d in use. If another controller payload is still running, stop it from its menu"),
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

    /* Without virtual pads there is nothing to connect controllers to. */
    g_vpad_ok = vpad_init();
    if (!g_vpad_ok) {
        notify(TR("AnyPad PS4: no se pueden crear mandos virtuales.\nVer el log", "AnyPad PS4: cannot create virtual controllers.\nSee the log"));
    } else {
        g_usb_pads = usb_pad_init(on_usb_feed, on_usb_attach, on_usb_detach);
        /* Controllers over the network (a phone/PC as the pad, the way
         * Control4Free does it): its own port and thread, feeding the same
         * virtual pads. Gives wireless pads without the console's own radio. */
        g_net_up = netpad_start(NETPAD_PORT);
        if (g_net_up)
            notify(TR("AnyPad PS4: mando por red en %s (abre esa pagina en el movil)",
                      "AnyPad PS4: network controller at %s (open that page on your phone)"), g_net_url);
        if (access(VPAD_TEST_FLAG, F_OK) == 0) {
            log_line("vpad test: one idle virtual pad on slot 0");
            if (vpad_add(0)) vpad_label(0, "test");
            g_vpad_test = 1;
        }
    }

    while (!g_stop && !g_web_stop) {
        long now;

        usleep(4000);
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

        /* The pages follow the network, as Payload Manager's does: every 5 s the
         * console's address is looked at, and the menu and the network
         * controller are served again if the address changed, the network was
         * lost, or a listener died (pulling the cable or changing the DNS
         * leaves it dead with the process alive). */
        if (now - last_net >= 5000) {
            char nip[16];
            int changed, net_dead;

            last_net = now;
            local_ip(nip);
            changed = strcmp(nip, ip) != 0;
            if (changed) {
                log_line("network: address changed from %s to %s", ip, nip);
                snprintf(ip, sizeof ip, "%s", nip);
                snprintf(g_net_url, sizeof g_net_url, "http://%s:%d/", ip, NETPAD_PORT);
            }
            if (changed || !g_web || !web_alive(g_web, WEB_PORT)) {
                if (g_web && !changed) log_line("network: the menu stopped answering; serving it again");
                if (g_web) web_stop(g_web);
                g_web = web_start(&wc, WEB_PORT);
            }
            /* Its clients went with the old address: they open the new one. */
            net_dead = g_vpad_ok && !netpad_alive();
            if (g_vpad_ok && (changed || net_dead)) {
                /* Only if it was up: one that never started (its port taken)
                 * is tried again quietly. */
                if (net_dead && !changed && g_net_up)
                    log_line("network: the network controller stopped answering; serving it again");
                netpad_stop();
                g_net_up = netpad_start(NETPAD_PORT);
            }
            if (changed && g_web && strcmp(nip, "127.0.0.1") != 0)
                notify(TR("AnyPad PS4: nueva direccion\nMenu: http://%s:%d/\nMando por red: %s",
                          "AnyPad PS4: new address\nMenu: http://%s:%d/\nNetwork controller: %s"), nip, WEB_PORT, g_net_url);
        }

        /* Rest mode on its way: leave cleanly before it, not after. */
        if (now - last_power >= 200) {
            int ps = power_state();
            last_power = now;
            if (ps == POWER_GOING_TO_REST || ps == POWER_IN_REST) {
                why = "rest mode";
                notify(TR("AnyPad PS4: parado por el modo reposo.\nLanzalo de nuevo al despertar", "AnyPad PS4: stopped by rest mode.\nRun it again after waking up"));
                break;
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
    if (g_vpad_ok) {
        int i;

        for (i = 0; i < MAX_PADS; i++) vpad_remove(i);    /* the test, USB and network ones */
        vpad_creds_restore();           /* leave the credentials as found */
    }
    lock_release();
    log_line("stopped");
    log_close();
    notify(TR("AnyPad PS4: parado", "AnyPad PS4: stopped"));
    return 0;
}
