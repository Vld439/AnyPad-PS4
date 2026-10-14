/* A simulated LE controller with one HID-over-GATT pad in range, behind
 * the hci_t interface: advertising, connection, SMP as responder (LE
 * Secure Connections or legacy), encryption checked against the agreed
 * key, and a GATT server with Device Information and HID services. */
#ifndef PH_SIM_LE_H
#define PH_SIM_LE_H

#include "../src/hci.h"

#include <stdint.h>

typedef struct sim_le sim_le_t;

typedef struct {
    uint8_t addr[6];            /* identity address, public */
    int use_rpa;                /* advertise resolvable private addresses */
    uint8_t irk[16];            /* as handed out: least significant byte first */
    int sc;                     /* the pad can do LE Secure Connections */
    uint16_t vid, pid;
    int no_pnp;                 /* no PnP ID characteristic */
    uint8_t map[600];           /* report map */
    int map_len;
    uint8_t report[64];         /* input report value, without its id */
    int report_len;
    uint8_t rid_in, rid_out;
    uint16_t mtu;               /* the pad's ATT MTU */
    int drop_every;             /* drop ~1 in N packets to the host */
    int wants_identity;         /* sends no input until the host hands over its identity
                                   (IRK and address) when pairing, like the Xbox Series */
    int needs_wake;             /* offers a HID Control Point and sends no input until
                                   it is told to leave suspend */
    int busy;                   /* someone else's LE connection attempt is pending in the
                                   controller: LE Create Connection is refused (0x0c) until
                                   it is cancelled this many times (it starts again after
                                   each cancel but the last) */
    unsigned seed;
} sim_le_pad;

sim_le_t *sim_le_new(const sim_le_pad *pad);
void      sim_le_free(sim_le_t *s);
hci_t     sim_le_hci(sim_le_t *s);
long      sim_le_now(void);

void sim_le_power_off(sim_le_t *s);     /* drops the link, stops advertising */
void sim_le_power_on(sim_le_t *s);      /* advertises again, new private address */
void sim_le_set_report(sim_le_t *s, const uint8_t *v, int n);
void sim_le_inject_acl(sim_le_t *s, unsigned cid, const uint8_t *d, int n);
void sim_le_inject_event(sim_le_t *s, uint8_t code, const uint8_t *p, int n);

int  sim_le_paired(const sim_le_t *s);          /* pairing completed */
int  sim_le_pairings(const sim_le_t *s);        /* pairing procedures run */
int  sim_le_encrypted(const sim_le_t *s);
int  sim_le_outputs(const sim_le_t *s);
int  sim_le_last_output(const sim_le_t *s, uint8_t *out, int max);
int  sim_le_blob_reads(const sim_le_t *s);
int  sim_le_identity(const sim_le_t *s);        /* the host's identity was received */
int  sim_le_creates(const sim_le_t *s);         /* LE Create Connection commands */
int  sim_le_cancels(const sim_le_t *s);         /* LE Create Connection Cancel commands */
const char *sim_le_error(const sim_le_t *s);    /* what the pad found wrong */

#endif
