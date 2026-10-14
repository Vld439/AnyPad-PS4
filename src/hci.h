/* The HCI transport: commands out, events and ACL packets in.
 *
 * The host stack only sees these operations, so the same code runs on the
 * console (hci_usb.c, the PS5's Bluetooth chip) and in the tests (a
 * simulated controller and pad).
 */
#ifndef PH_HCI_H
#define PH_HCI_H

#define HCI_PKT_MAX 1100

typedef struct hci_ops {
    /* Sends an HCI command. Returns 0 on a transport error. */
    int  (*cmd)(void *ctx, unsigned opcode, const void *params, int plen);
    /* Sends one ACL packet, 4-byte header included. Returns 0 on error. */
    int  (*acl_send)(void *ctx, const unsigned char *pkt, int len);
    /* Waits up to timeout_ms for incoming packets. Returns 1 if any queued,
     * 0 if none, -1 if the transport is gone (the device went away, as it
     * does across rest mode). */
    int  (*pump)(void *ctx, int timeout_ms);
    /* Pops one queued packet; returns its length or 0 if none. Events come
     * as code, length, parameters; ACL packets with their 4-byte header. */
    int  (*next_event)(void *ctx, unsigned char *out, int max);
    int  (*next_acl)(void *ctx, unsigned char *out, int max);
    void (*close)(void *ctx);
    /* Optional: writes what the transport knows about itself to the log
     * (device, traffic counts, samples). Called once when the controller
     * does not answer, to find out why. NULL: nothing to say. */
    void (*diag)(void *ctx);
    /* Optional: stops taking the controller's events for ms milliseconds, so
     * that the system's driver, which reads the same endpoint, gets them all
     * meanwhile (ACL data keeps coming). NULL: not supported. */
    void (*yield)(void *ctx, int ms);
    /* Optional: how many of the reads it wanted could not be opened because
     * another process holds the controller's transfers (0: all of them, or
     * not supported). The transport keeps trying to open them. */
    int  (*missing_reads)(void *ctx);
    /* Optional: keeps at most n event reads pending (0 to all). The events
     * nobody of ours waits for are then left to the system's own reads. */
    void (*event_reads)(void *ctx, int n);
    /* Optional: the length asked of each event read. One packet (the
     * default) splits a long event among whoever reads; a larger one ends on
     * the event's short last packet, so each read holds whole events. */
    void (*event_len)(void *ctx, int n);
    /* Optional: the chip's USB vendor id (0x0E8D MediaTek, 0x1286
     * Marvell/NXP), 0 if not known. NULL: not known. */
    int  (*chip_vendor)(void *ctx);
} hci_ops;

typedef struct {
    const hci_ops *ops;
    void *ctx;
} hci_t;

#endif
