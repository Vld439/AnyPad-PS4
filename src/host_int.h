/* The host's internals, shared by the Classic (host.c) and LE (le.c) halves
 * of the stack. Not for use outside them. */
#ifndef PH_HOST_INT_H
#define PH_HOST_INT_H

#include "host.h"
#include "profiles.h"

#include <stdint.h>

/* ---- protocol constants ------------------------------------------------- */

/* HCI opcodes: OGF << 10 | OCF. */
#define OP_INQUIRY              0x0401
#define OP_INQUIRY_CANCEL       0x0402
#define OP_CREATE_CONNECTION    0x0405
#define OP_DISCONNECT           0x0406
#define OP_ACCEPT_CONNECTION    0x0409
#define OP_REJECT_CONNECTION    0x040A
#define OP_LINK_KEY_REPLY       0x040B
#define OP_LINK_KEY_NEG_REPLY   0x040C
#define OP_PIN_CODE_REPLY       0x040D
#define OP_AUTH_REQUESTED       0x0411
#define OP_SET_ENCRYPTION       0x0413
#define OP_IO_CAP_REPLY         0x042B
#define OP_USER_CONFIRM_REPLY   0x042C
#define OP_SET_EVENT_MASK       0x0C01
#define OP_WRITE_LOCAL_NAME     0x0C13
#define OP_WRITE_SCAN_ENABLE    0x0C1A
#define OP_READ_PAGE_SCAN_ACT   0x0C1B
#define OP_WRITE_PAGE_SCAN_ACT  0x0C1C
#define OP_WRITE_CLASS_OF_DEV   0x0C24
#define OP_WRITE_INQUIRY_MODE   0x0C45
#define OP_WRITE_SSP_MODE       0x0C56
#define OP_READ_BUFFER_SIZE     0x1005
#define OP_READ_BD_ADDR         0x1009
#define OP_READ_SCAN_ENABLE     0x0C19
#define OP_READ_SSP_MODE        0x0C55

#define CID_SIGNALING   0x0001
#define PSM_SDP         0x0001
#define PSM_HID_CTRL    0x0011
#define PSM_HID_INTR    0x0013

/* Our ends of a link's channels (CIDs are per link). */
#define CID_SDP         0x0040
#define CID_CTRL        0x0041
#define CID_INTR        0x0042

/* L2CAP signaling codes. */
#define SIG_REJECT      0x01
#define SIG_CONN_REQ    0x02
#define SIG_CONN_RSP    0x03
#define SIG_CFG_REQ     0x04
#define SIG_CFG_RSP     0x05
#define SIG_DISC_REQ    0x06
#define SIG_DISC_RSP    0x07
#define SIG_ECHO_REQ    0x08
#define SIG_ECHO_RSP    0x09
#define SIG_INFO_REQ    0x0A
#define SIG_INFO_RSP    0x0B

/* HID transaction headers. */
#define HID_DATA_INPUT  0xA1
#define HID_DATA_OUTPUT 0xA2
#define HID_CONTROL_UNPLUG 0x15

/* ---- timing (ms) -------------------------------------------------------- */

#define T_SELF_AUTH      800    /* a reconnecting pad may authenticate itself */
#define T_SELF_ENCRYPT  1500    /* ... and start encryption itself */
#define T_SELF_CHANNELS 1200    /* ... and open its own HID channels */
#define T_INTR_AFTER     500    /* interrupt channel after control */
#define T_CONN_TIMEOUT 20000    /* paging a pad */
#define T_SETUP_TIMEOUT 30000   /* connection to ready */
#define T_YIELD         30000   /* events left to the system after a call that is not ours: longer than a
                                   * DualSense's ~22 s between attempts, so its next one reaches the system */
#define T_CHAN_CONNECT  2500
#define T_CFG_RESEND    1200
#define CFG_TRIES          6
#define T_CFG_TIMEOUT   4000    /* the pad's own request may have been lost */
#define T_SDP_RESEND    2000
#define SDP_TRIES          3
#define T_INIT_STEP       60    /* between wake-up reports */
#define T_INIT_RETRY     800    /* before repeating an unanswered sequence */
#define INIT_ROUNDS        5
#define T_INIT_SLOW     3000    /* after INIT_ROUNDS: keep asking, more slowly, until it switches */

#define SDP_Q_IDS          1    /* Device ID: vendor and product */
#define SDP_Q_DESC         2    /* HID report descriptor, for the generic profile */
#define SDP_RECORDS        8    /* service records looked at when asking in two steps */
#define T_PROBE_IDLE   20000    /* nothing heard for this long: probe */
#define T_SSP_STALL      3000   /* pairing step answered but the next event never came: answer the next step anyway */
#define REPAGE_TRIES        2   /* extra calls to a pad that did not answer the first one (page timeout) */
#define T_REPAGE_WAIT     500   /* between a failed call and the next */
#define PS_PAIR_INTERVAL 0x0800 /* page scan while pairing: every 1.28 s (R1)... */
#define PS_PAIR_WINDOW   0x0012 /* ...for 11.25 ms, the system's own light window */
#define T_PS_CHECK       3000   /* while pairing, how often the page scan is looked at again */
#define HOST_EV_READS      40   /* event reads the USB transport keeps (all of them) */
#define T_PS_CHECK_IDLE  30000  /* with pads connected and no pairing, how often the page scan is looked at */
#define TURNDOWN_TRIES      3   /* calls of the pad itself turned down in one search, to call it instead */
#define T_TURNDOWN_WAIT  2000   /* between turning its call down and calling it (0.6.19: 700 was too soon for the 8BitDo) */
#define FRESH_LINK_TRIES    1   /* links dropped and made again in one search when the pad has no resources for a channel */
#define T_FRESH_LINK     3000   /* between dropping such a link and calling again */
#define T_EIR_WAIT     20000    /* a pad found with an empty answer: wait this long for a complete one before calling it */
#define T_PROBE_WAIT    2000    /* between probes */
#define PROBE_TRIES        3

/* ACL flow control on the shared controller: a few Number Of Completed
 * Packets reports go to the system's driver, so a packet without a report
 * after INFLIGHT_MS is taken as sent. */
#define INFLIGHT_MAX      64
#define INFLIGHT_MS       60

#define DB_MAX            16
#define DB_REC            64    /* one record in the store file */
#define DB_MAGIC     "ANYPAD02"

/* ---- state -------------------------------------------------------------- */

typedef enum { CH_CLOSED, CH_CONNECTING, CH_CONFIG, CH_OPEN } chan_state;

typedef struct {
    unsigned psm, scid, dcid;
    chan_state st;
    int cfg_ours_ok;        /* the pad accepted our configuration */
    int cfg_theirs_ok;      /* we accepted the pad's */
    int cfg_tries;
    unsigned char early_cfg_id;
    int early_cfg; /* default configuration received before connection response */
    long t_state, t_cfg;
    unsigned refused;
    long retry_at;
} chan;

/* A paired pad. Classic pads keep a link key; LE pads a long-term key
 * (with its EDIV and Rand under legacy pairing) and possibly an identity
 * resolving key, to recognise them behind private addresses. */
typedef struct {
    unsigned char addr[6], key[16], type;
    uint16_t vid, pid;
    uint8_t le, addr_type;
    uint16_t ediv;
    uint8_t rand[8], irk[16], has_irk;
} db_rec;

typedef struct {
    int credits, max;
    long inflight[INFLIGHT_MAX];
    int head, count;
} acl_pool;

#define LE_MAX_CHARS 24

typedef struct {
    uint16_t decl, value, end, uuid;
    uint8_t props;
    uint8_t rid, rtype;             /* from its Report Reference */
    uint16_t ref, cccd;             /* descriptor handles, 0 if none */
} gatt_char;

/* An LE pad's pairing (SMP) and GATT discovery. Crypto values are kept
 * most significant byte first, as the specification writes them. */
typedef struct {
    uint8_t addr_type;

    int smp;                        /* SMP_* */
    int sc;                         /* LE Secure Connections */
    uint8_t preq[7], pres[7];       /* the pairing commands, as sent */
    uint8_t na[16], nb[16], cb[16];
    uint8_t pkb[64];                /* the pad's public key, as sent */
    uint8_t dhkey[32];              /* least significant byte first */
    int have_pkb, have_cb, have_nb, have_dhkey, dhkey_asked;
    uint8_t ltk[16];                /* the key the link is encrypted with */
    uint8_t keys_expected, keys_got;
    uint8_t dist_ltk[16], dist_rand[8], dist_irk[16], id_addr[6], id_type;
    uint16_t dist_ediv;
    int enc_sent, bonded;
    int identity_sent;              /* our identity (IRK and address) handed to the pad */
    int suspend_off;                /* HID Control Point: Exit Suspend written */
    int create_out;                 /* our LE Create Connection awaits its status */
    int refused;                    /* times it was refused as busy (0x0c) */
    long retry_at;                  /* when to ask again after a cancel, 0 if not */
    long t_smp, t_enc;

    int gatt;                       /* G_* */
    uint16_t mtu;
    uint8_t req[32];
    int req_len, req_tries;
    long t_req;
    uint16_t svc_start, svc_end, cursor;
    gatt_char ch[LE_MAX_CHARS];
    int nch, idx;
    uint16_t map_handle;
    uint8_t map[1024];
    int map_len;
} le_link;

typedef struct {
    int used;
    unsigned char addr[6];
    int connected;          /* the ACL link is up */
    unsigned handle;
    int pairing;            /* we found it in pairing mode */
    long t_create, t_conn;

    int auth_sent, auth_ok, enc_sent, enc_on;
    int ssp_stage;                  /* pairing steps we answered: 1 key refused, 2 capabilities given, 3 confirmed */
    long t_ssp;                     /* when the last one was answered */
    long t_auth_ok, t_secure;

    int ids_known;
    uint16_t vid, pid;
    const pad_profile *prof;

    int sdp_query;                  /* SDP_Q_* being asked, 0: none */
    int sdp_sent, sdp_tries;
    unsigned sdp_tid;
    long t_sdp;
    unsigned char sdp_cont[16];     /* continuation of a long answer */
    int sdp_cont_len;
    unsigned char sdp_buf[1024];    /* attribute lists gathered so far */
    int sdp_len;
    /* Pads that do not answer the one-step query (search and attributes at
     * once) are asked in two: which records match, then each one's attributes. */
    int sdp_two_step;               /* this pad needs it (decided once per link) */
    int sdp_have_records;           /* the record handles are known */
    uint32_t sdp_rec[SDP_RECORDS];
    int sdp_nrec, sdp_irec;         /* handles known, the one being read */

    chan sdp, ctrl, intr;
    int ready, disconnecting;

    unsigned char rx[1024];
    int rx_len, rx_need;

    pad_state st;
    pad_output out;
    long t_out;

    /* The profile's own memory, and its wake-up sequence. */
    union { unsigned char bytes[PROFILE_CTX_MAX]; uint64_t align; } pctx;
    int init_step, init_rounds, init_done, full;
    long t_init;

    int le;                         /* a Bluetooth LE link */
    le_link le_s;

    char name[32];                  /* the name it gave in the search (EIR), "" if none */
    uint32_t cod;                   /* its class of device, 0 if not known */
    int announced;                  /* its virtual DualSense exists: created at its first input */
    unsigned char seen_id[4];       /* the report ids it has sent, for the log */
    int seen_n[4];
    int pin_zero;                   /* "0000" was its PIN (legacy pairing) */
} link_t;

struct host {
    hci_t hci;
    host_events ev;
    char db_path[256];
    char map_dir[256];
    char ban_path[256];             /* ban.txt: addresses the search must never look at */
    unsigned char ban[16][6];
    int n_ban;
    int discovery_blocked;          /* cannot safely remember another system pad */
    unsigned char collision_addr[6];
    int collision_blocked;          /* retry only on an explicit new search */
    unsigned bad_inquiry;           /* malformed discovery events rejected */
    db_rec db[DB_MAX];
    int ndb;

    link_t links[HOST_MAX_PADS];
    unsigned char sig_id;

    unsigned cc_op;
    unsigned char cc[HCI_PKT_MAX];
    int cc_len;

    unsigned acl_mtu;
    acl_pool br;                    /* the controller's ACL buffers */

    /* Bluetooth LE (le.c) */
    int le_ok;                      /* the controller does LE */
    int le_separate;                /* LE has its own buffers */
    unsigned le_mtu;
    acl_pool lep;
    unsigned char bdaddr[6];        /* our address */
    unsigned char le_pk[64];        /* our P-256 public key, as sent */
    int le_pk_ok;
    uint8_t le_irk[16];             /* this console's identity key, kept in <db>.irk */
    uint8_t le_rest_addr[6];        /* an LE pad left alone for a while after refusals */
    uint8_t pin_addr[6];            /* a pad that refused "0000": its next PIN is our address */
    int pin_addr_set;
    long le_rest_until;
    int le_irk_ok;
    int le_scanning, le_scan_want;
    long t_scan;
    link_t *dhkey_owner;            /* the link a DHKey is being made for */

    long pair_until;
    long purge_at;                  /* when stale links were last cleared */
    long yield_at;                  /* when events were last left to the system */
    int inquiring;
    unsigned char defer_rec[14];    /* a pad found whose extended answer came empty (cut short) */
    long defer_at;                  /* since when it waits for a complete one; 0: none */
    unsigned char last_call[14];    /* the answer the last call was made from */
    int repage_left, in_repage;     /* extra calls still allowed; the one being made is one of them */
    long repage_at;                 /* when the next extra call is due; 0: none */
    int pair_pending_s;             /* «Pair» pressed during a pause for the system: search this long once it ends */
    long pair_wait_until;           /* when that pause ends */
    long search_started;            /* when the current search really began (to time how long finding a pad takes) */
    long inq_started;               /* when the inquiry in progress began, for the watchdog */
    int dead;               /* the transport is gone */
    int diag_done;          /* the transport's self-report was logged */
    int scan_orig, scan_set; /* page scan as found, and as we set it (-1: untouched) */
    int ps_saved;            /* page scan lightened for pairing: the system's value is below */
    unsigned ps_orig_iv, ps_orig_win;
    long ps_check_at;        /* when to look at the page scan again while pairing */
    int ps_lightened;        /* times it was lightened in this run */
    int round_heard;         /* devices heard in the inquiry in progress */
    int mask_set;            /* our event mask is on the controller */
    unsigned ps_set_iv, ps_set_win; /* the light page scan we wrote */
    long ps_restore_at;      /* putting it back failed: try again then */
    int reads_now;           /* event reads the transport keeps now */
    long full_reads_until;   /* a command was sent: all reads until its reply can come */
    int turned_down;         /* calls of the pad itself turned down in this search */
    int fresh_left;          /* links that may still be made again in this search */
    int reject_pending;      /* one of them was just turned down: is it really a call? */
    unsigned char reject_addr[6];
    int ps_restore_tries;
    long last_rx, probe_at; /* liveness: anything heard, last probe */
    int probes;
};


/* Dialog mode: settings that can be changed while AnyPad runs (web /api/tune). */
typedef struct {
    int ps_light;               /* lighten the system's page scan while pairing */
    unsigned ps_iv, ps_win;     /* the light page scan */
    int mask_conn;              /* leave calls to the console (Connection Request) to the system */
    int inq_len;                /* inquiry length, in units of 1.28 s */
    int trace;                  /* write every HCI event and command to the log */
    int idle_reads;             /* event reads kept while not pairing (all: 40) */
    int ev_len;                 /* bytes asked of each event read (16: one packet, as always) */
} tune_t;
extern tune_t g_tune;

/* ---- shared between host.c and le.c ---- */

const char *addr_str(const unsigned char *a);
int  send_cmd(host_t *h, unsigned op, const void *p, int len);
int  hci_sync(host_t *h, unsigned op, const void *p, int plen);
int  slot_of(host_t *h, const link_t *l);
link_t *link_by_addr(host_t *h, const unsigned char *a);
link_t *link_new(host_t *h, const unsigned char *addr);
void link_free(host_t *h, link_t *l);
void link_disconnect(host_t *h, link_t *l, const char *why);
void link_ready(host_t *h, link_t *l);
void link_input(host_t *h, link_t *l, const unsigned char *rep, int len);
int  l2_send(host_t *h, link_t *l, unsigned cid, const unsigned char *d, int len);
void init_tick(host_t *h, link_t *l, long now);

db_rec *db_find(host_t *h, const unsigned char *addr);
db_rec *db_put(host_t *h, const unsigned char *addr);
void db_save(host_t *h);
void db_forget(host_t *h, const unsigned char *addr);

/* le.c */
void le_setup(host_t *h);
void le_on_meta(host_t *h, const unsigned char *ev, int len);
void le_on_frame(host_t *h, link_t *l, unsigned cid, const unsigned char *d, int len);
void le_on_encryption(host_t *h, link_t *l, int status, int on);
void le_step(host_t *h, link_t *l, long now);
void le_poll(host_t *h, long now);
void le_shutdown(host_t *h);
void le_on_create_status(host_t *h, int status);
int  le_send_report(host_t *h, link_t *l, const unsigned char *rep, int len);

#endif
