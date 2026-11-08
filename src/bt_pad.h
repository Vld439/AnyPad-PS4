/* DualSense over the console's own Bluetooth radio, as an opt-in.
 *
 * The console's Bluetooth stack only hands a controller's HID channel to
 * userland if the pad's vendor and product id are in a list built into the
 * driver. OneFamily (github.com/ChiricoG/OneFamily, MIT) found the way: patch
 * that list in kernel memory, register 054C:0CE6 with the driver, and read the
 * pad's reports by address. bt_proto.c documents the reports; this file does
 * the kernel side and the polling.
 *
 * What is different here: the driver sizes each link's output report for a
 * 22-byte pad, and a DualSense drops the link if a 78-byte report comes
 * through it. The per-link capacity (at link + 0x1574) is raised to 78 for
 * each pad that connects, so rumble, light bar and the extended 0x31 input
 * reports work on 13.52.
 *
 * Only runs when asked for: bt_pad_init() is called when the menu's option is
 * on, and bt_pad_close() puts the kernel back. One copy of this at a time
 * (AnyPad's instance lock), and not together with OneFamily, which patches
 * the same words.
 *
 * The pads become virtual DualShock 4s like the USB ones, taking the highest
 * free slots. A pad that goes quiet keeps its slot for a while, fed neutral,
 * so the console does not drop the port; after the grace period the slot is
 * released.
 */
#ifndef ANYPAD_BT_PAD_H
#define ANYPAD_BT_PAD_H

#include "bt_proto.h"
#include "pad.h"

typedef struct bt_pad_mgr_s bt_pad_mgr_t;
typedef void (*bt_pad_feed_fn)(int slot, const pad_state *st);
typedef void (*bt_pad_attach_fn)(int slot, const char *name);
typedef void (*bt_pad_detach_fn)(int slot);

/* Opens /dev/bt, patches the driver's list and registers the ids of `family`
 * (NULL: the default one, the DualSense). Only one family at a time: the
 * driver names a device by address only, never by vendor and product id.
 * NULL if the console cannot do it (firmware other than 13.52, no /dev/bt,
 * patch refused), with the reason in the log. */
bt_pad_mgr_t *bt_pad_init(const bt_family *family,
                          bt_pad_feed_fn on_feed,
                          bt_pad_attach_fn on_attach,
                          bt_pad_detach_fn on_detach);

/* Reads what the driver has, without blocking. Call every few milliseconds. */
void bt_pad_poll(bt_pad_mgr_t *mgr, long now);

/* What the console asks of the pad in `slot` (rumble, light bar): sent to it
 * over Bluetooth. Not available yet: no game output reaches AnyPad's pads, so
 * only the neutral reports that keep the link come out. */
void bt_pad_output(bt_pad_mgr_t *mgr, int slot, const pad_output *out);

/* The family the module was started with ("sony", "switch"). */
const char *bt_pad_family(const bt_pad_mgr_t *mgr);

/* Gives every connected pad its link's capacity back, unregisters, restores
 * the driver's list and closes. The virtual pads are the caller's to remove. */
void bt_pad_close(bt_pad_mgr_t *mgr);

#endif
