/* Each connected controller becomes a virtual DualShock 4 on the console.
 *
 *   0. The process gets system-service credentials (checked, read back).
 *   1. scePadVirtualDeviceAddDevice({32, user 1, ...}, 1) creates an MBus
 *      device. Its return value is a status, not a handle.
 *   2. The kernel log announces the device:
 *        SCE_MBUS_EVENT_DEVICE_ADDED [DeviceId:0x...][type:1][subType:2]
 *   3. scePadVirtualDeviceInsertData(DeviceId & 0xffffffff, &ScePadData)
 *      feeds it. It is not bound to a user: the console gives it an owner
 *      itself, or asks who is using it.
 *
 * Only a device identified without doubt is ever fed or deleted: one
 * DEVICE_ADDED line, and only one, between our AddDevice and a few seconds
 * later. Anything else is logged and left alone.
 */
#ifndef PH_PS4_VPAD_H
#define PH_PS4_VPAD_H

#include "pad.h"

#include <stddef.h>

/* Raises this process's credentials (checked) and starts libScePad.
 * Returns 0 if any step fails; virtual pads then stay off. */
int  vpad_init(void);

/* Asks for a virtual pad for `slot`. It is created and identified by
 * vpad_poll() without blocking the caller; vpad_live() says when it is. */
int  vpad_add(int slot);
void vpad_poll(long now);
int  vpad_live(int slot);

/* What drives the slot, for the menu. Set after vpad_add, which clears it. */
void vpad_label(int slot, const char *label);
/* A live slot's label and latest state. Returns 0 if the slot is not live. */
int  vpad_get(int slot, char *label, size_t max, pad_state *st);

void vpad_update(int slot, const pad_state *st);

/* Holds PS on the virtual pad for `ms` milliseconds (the menu's PS button).
 * 0 if the slot has no live virtual pad. */
int  vpad_press_ps(int slot, int ms);
void vpad_remove(int slot);

/* Gives the process its original credentials back, and raises them again.
 * Both check what they did. */
int  vpad_creds_restore(void);
int  vpad_creds_raise(void);

void notify(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
