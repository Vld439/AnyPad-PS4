/* DualSense over the console's own Bluetooth radio: the reports read from
 * /dev/bt and the output report the pad expects.
 *
 * The console's Bluetooth stack only opens a controller's HID channel if its
 * vendor and product id are in a list built into the driver; bt_pad.c patches
 * that list and registers 054C:0CE6. What comes back is not a HID stream but
 * one event per device, and its report is fetched by address:
 *
 *   read(/dev/bt)            16-byte events, types 0x18..0x20, the device's
 *                            kernel address in [8..15]
 *   ioctl(0x80206233, ...)   that address's latest report: one length byte,
 *                            a 0x1F marker, then the HID report itself
 *   ioctl(0x80206232, ...)   writes a report to that address
 *
 * The parsing of the report belongs to the controller's profile: the 0x31
 * input report is the USB one with a CRC appended, see ds5_bt_parse().
 */
#ifndef PH_BT_PROTO_H
#define PH_BT_PROTO_H

#include "pad.h"

#include <stdint.h>

#define BT_EVENT_LEN 16
#define BT_REPORT_MAX 80

/* Event types the driver reports; the ones below 0x18 carry no device. */
#define BT_EVENT_FIRST 0x18
#define BT_EVENT_LAST 0x20

/* A device event: 1 if it is one, with the address it names. */
int bt_event_address(const uint8_t *ev, uint64_t *address);

/* The HID report inside what ioctl 0x80206233 returned, or 0 if the buffer is
 * not one: buf[0] + 2 bytes, with the report after a 0x1F marker. */
int bt_report_unwrap(const uint8_t *buf, int n, const uint8_t **rep);

/* 1 if `rep` is a report a DualSense can have sent: 0x31 with a valid CRC
 * (0xA1 over 74 bytes), or the 10-byte 0x01 it sends until the first output
 * report. */
int bt_input_ok(const uint8_t *rep, int len);

/* Per-pad state for bt_output_build(): the sequence nibble the pad watches,
 * and whether the report that ends its pairing animation has gone out. */
typedef struct {
    uint8_t seq;
    uint8_t started;
} bt_out;

/* Builds the 78-byte output report 0x31 (id first, CRC last) into `buf`:
 * the pad's motors, its light bar and player LEDs, and the sequence nibble.
 * The first report only ends the pairing animation, as the pad ignores
 * everything else until it has seen one. The layout is OneFamily's
 * (daemon/src/bt_driver.c, MIT), proven on a DualSense over Bluetooth.
 * Returns 78, or 0 if `max` is too small. */
int bt_output_build(bt_out *o, const pad_output *out, uint8_t *buf, int max);

#endif
