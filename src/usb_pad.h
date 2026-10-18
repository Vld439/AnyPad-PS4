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
void usb_pad_close(usb_pad_mgr_t *mgr);

#endif
