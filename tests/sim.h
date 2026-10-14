/* A simulated Bluetooth controller with one game pad in range, behind the
 * hci_t interface. It answers HCI commands as a controller would, and
 * plays the pad's side of pairing, L2CAP, SDP and HID, so the host stack
 * can be exercised end to end without hardware. */
#ifndef PH_SIM_H
#define PH_SIM_H

#include "../src/hci.h"

#include <stdint.h>

typedef struct sim sim_t;

typedef enum {
    SIM_WAIT_PAIRING,   /* in pairing mode: waits for the host to find it */
    SIM_RECONNECT,      /* paired before: connects to the host by itself */
} sim_mode;

typedef struct {
    sim_mode mode;
    unsigned char addr[6];
    uint32_t cod;
    uint16_t vid, pid;
    /* The input report the pad streams once its interrupt channel is open
     * and it has seen an output report (or at once, if !needs_output). */
    unsigned char report[128];
    int report_len;
    int needs_output;
    int early_cfg, no_cfg_repeat;
    int refuse_count; /* -1 forever; otherwise this many initial requests */
    int refuse_first_link; /* every channel refused (no resources) on the first link only */
    /* Output reports ignored before it switches to its full report (a pad that
     * misses the first wake-up rounds). 0: switches at the first one. */
    int ignore_outputs;
    /* Switches to its full report only when this feature report is asked
     * for on the control channel (GET_REPORT), whatever the output reports
     * say: some DualShock 4 clones. 0: no such need. */
    int wake_feature;
    /* Answers the Device ID question with nothing (some clones). */
    int no_device_id;
    /* Legacy pairing with a PIN: 1 wants the host's address (a Wii pad
     * paired with Sync), 2 wants "0000". 0: Secure Simple Pairing. */
    int legacy_pin;
    /* Nintendo's Wii protocol: 1 a Wii U Pro (an extension, woken by two
     * register writes, read in mode 0x34), 2 a Wii Remote alone. basic[0..1]
     * are its core buttons, report[0..18] the extension bytes. */
    int wii;
    /* Answers SDP only in two steps (Service Search, then Service Attribute);
     * the one-step Service Search Attribute gets an error. */
    int sdp_two_step_only;
    /* Streamed before the first output report, if any (a pad's simple
     * mode before it is woken up). */
    unsigned char basic[64];
    int basic_len;
    /* The HID report descriptor its SDP record serves, in pieces. */
    unsigned char desc[512];
    int desc_len;
    /* Drop every Nth event and ACL packet, as the system's driver would
     * take them. 0: none. */
    int drop_every;
    /* Extended inquiry response bytes sent with the pad's search result. */
    unsigned char eir[64];
    int eir_len;
    /* The first N inquiry answers come cut short (no data), as when another
     * reader of the chip takes the rest of the event. */
    int eir_empty_rounds;
    unsigned seed;
} sim_pad;

sim_t *sim_new(const sim_pad *pad);
void   sim_free(sim_t *s);
hci_t  sim_hci(sim_t *s);

/* Virtual clock, advanced by each pump. */
long   sim_now(void);
int    sim_diag_calls(void);
int    sim_yields(void);       /* times the host left the events to the system */   /* times the host asked the transport for its self-report */

void   sim_power_on(sim_t *s);      /* SIM_RECONNECT: page the host */
void   sim_disconnect(sim_t *s);    /* the pad switches off */
void   sim_vanish(sim_t *s);        /* the link drops with no event reaching the host; it will call again */
void   sim_set_report(sim_t *s, const unsigned char *r, int len);
void   sim_give_key(sim_t *s, const unsigned char *key);   /* paired before */
void   sim_go_deaf(sim_t *s);
/* Garbage from the air: frames and events no well-behaved pad sends. */
void   sim_inject_acl(sim_t *s, unsigned cid, const unsigned char *d, int n);
void   sim_inject_event(sim_t *s, unsigned char code, const unsigned char *p, int n);    /* stops answering, as after rest mode */

/* What the host did. */
int    sim_sdp_queries(const sim_t *s);
int    sim_outputs(const sim_t *s);
int    sim_feature_requests(const sim_t *s);   /* GET_REPORT (feature) on the control channel */
int    sim_pins(const sim_t *s);               /* PIN replies received */
int    sim_pin_ok(const sim_t *s);             /* the last one was the PIN it wanted */
void   sim_wii_status(sim_t *s);              /* a Wii pad sends a status report and stops until its mode is set */
int    sim_inquiries(const sim_t *s);      /* Inquiry commands the host has sent */
int    sim_unplugs(const sim_t *s);
int    sim_rejects(const sim_t *s);        /* Reject Connection Request commands received */        /* HID 'virtual cable unplug' messages received */
int    sim_sdp_continuations(const sim_t *s);
int    sim_last_output(const sim_t *s, unsigned char *out, int max);
int    sim_link_up(const sim_t *s);
int    sim_dropped(const sim_t *s);
int    sim_scan(const sim_t *s);           /* the controller's scan enable */
void   sim_set_scan(sim_t *s, int v);     /* someone else changes it */
void   sim_stale_link(sim_t *s, unsigned handle);   /* the chip still holds the pad's link from an earlier run */
int    sim_stale_cleared(const sim_t *s);
int    sim_early_calls(const sim_t *s);
int    sim_calls(const sim_t *s);
void   sim_fail_pages(sim_t *s, int n);        /* the next n calls to the pad get no answer (page timeout) */
void   sim_set_page_scan(sim_t *s, unsigned iv, unsigned win);
void   sim_get_page_scan(const sim_t *s, unsigned *iv, unsigned *win);
void   sim_busy_radio(sim_t *s, int on);        /* with a heavy page scan, searches hear nothing and calls time out */
const unsigned char *sim_event_mask(const sim_t *s);
int    sim_event_reads(const sim_t *s);           /* event reads the host keeps pending */
void   sim_pad_calls_pending(sim_t *s);           /* the pad called the console earlier and its call waits (unanswered) */
int    sim_left_to_system(const sim_t *s);       /* events that found none of ours pending */
void   sim_drop_events(sim_t *s, int code, int n);   /* the next n events with this code never reach the host */          /* Create Connection commands for the pad */    /* calls made before a complete answer was sent */

int sim_disconnect_requests(const sim_t *s);
int sim_inquiry_during_setup(const sim_t *s);

int sim_create_requests(const sim_t *s);

int sim_channel_requests(const sim_t *s);
unsigned sim_tx_hash(const sim_t *s);
int sim_cfg_responses(const sim_t *s);

#endif
