#ifndef ANYPAD_USB_PAD_H
#define ANYPAD_USB_PAD_H

#include "pad.h"

typedef struct usb_pad_mgr_s usb_pad_mgr_t;
typedef void (*usb_pad_feed_fn)(int slot, const pad_state *st);
typedef void (*usb_pad_attach_fn)(int slot, const char *name);
typedef void (*usb_pad_detach_fn)(int slot);

usb_pad_mgr_t *usb_pad_init(usb_pad_feed_fn on_feed,
                             usb_pad_attach_fn on_attach,
                             usb_pad_detach_fn on_detach);
void usb_pad_poll(usb_pad_mgr_t *mgr, long now);

/* What the console asks of the pad in `slot` (rumble, light bar): sent through
 * the pad's own output report. 1 if a pad took it. */
int usb_pad_output(usb_pad_mgr_t *mgr, int slot, const pad_output *out);
void usb_pad_close(usb_pad_mgr_t *mgr);

#endif
