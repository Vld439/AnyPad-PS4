/* Which endpoints carry Bluetooth, read from the chip's own configuration
 * descriptor instead of assumed.
 *
 * The PS5's Bluetooth chip (MediaTek 0e8d:3603) has two HCI functions. Each is
 * a USB interface of class e0/01/01 with an interrupt-IN endpoint (HCI
 * events), a bulk-IN endpoint (ACL from the chip) and a bulk-OUT endpoint
 * (ACL to the chip); a second interface with isochronous endpoints carries
 * voice. Their addresses differ between consoles: on a PS5 fat with firmware
 * 10.01 the events come on 0x81 and the ACL on 0x82, which cost two trials
 * to find out. The rule here is the usual one: the first endpoint of each
 * kind in descriptor order. */
#ifndef ANYPAD_USB_DESC_H
#define ANYPAD_USB_DESC_H

#include <stdint.h>

#define HCI_FUNCTIONS_MAX 4

typedef struct {
    int iface;                      /* bInterfaceNumber */
    uint8_t ep_events, ep_acl_in, ep_acl_out;       /* endpoint addresses, 0: not there */
    uint16_t mps_events, mps_acl_in, mps_acl_out;   /* wMaxPacketSize */
    int n_other;                    /* endpoints of the interface not used */
    uint8_t other[8];               /* ... their addresses, for the log */
} hci_function;

/* Finds the HCI functions (interface class e0/01/01, alternate setting 0, with
 * an interrupt-IN and both bulk endpoints), in descriptor order. Returns how
 * many were found (at most HCI_FUNCTIONS_MAX). */
int usb_find_hci_functions(const uint8_t *cfg, int len, hci_function *out);

/* The second bulk OUT of the function to send ACL on, or 0 to use the
 * first. Only one numbered like the bulk IN (0x02 with 0x82), when the
 * first is not: on the MediaTek chip the first OUT (0x01) runs what is
 * written to it as an HCI command, and ACL belongs on 0x02. Other chips
 * (Marvell/NXP) keep the first OUT, as programs that work on them do. */
uint8_t usb_spare_acl_out(const hci_function *f);

#endif
