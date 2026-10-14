/* Each connected Bluetooth pad becomes a virtual DualSense on the console,
 * bound to the main user (the one in the foreground, who ran the jailbreak).
 *
 * The PS5 path:
 *
 *   0. The process gets system-service credentials (checked, read back).
 *   1. libSceMbus is loaded before any scePad call (libScePad imports it).
 *   2. scePadVirtualDeviceAddDevice({32, user 1, ...}, DualSense) creates an
 *      MBus device. Its return value is a status, not a handle.
 *   3. The kernel log announces the device:
 *        SCE_MBUS_EVENT_DEVICE_ADDED [DeviceId:0x...][type:1][subType:22]...
 *   4. sceMbusBindDeviceWithUserId(DeviceId, user) gives it to the user.
 *   5. scePadVirtualDeviceInsertData(DeviceId & 0xffffffff, &ScePadData)
 *      feeds it.
 *
 * Only a device identified without doubt is ever bound, fed or deleted:
 * one DEVICE_ADDED line, and only one, between our AddDevice and a few
 * seconds later. Anything else is logged and left alone.
 */
#ifndef PH_PS5_VPAD_H
#define PH_PS5_VPAD_H

#include "pad.h"

/* Raises this process's credentials (checked), loads MBus and libScePad.
 * Returns 0 if any step fails; virtual pads then stay off. */
int  vpad_init(void);

/* Asks for a virtual pad for `slot`. It is created and identified by
 * vpad_poll() without blocking the caller; vpad_live() says when it is. */
int  vpad_add(int slot);
void vpad_poll(long now);
int  vpad_live(int slot);

void vpad_update(int slot, const pad_state *st);

/* Holds PS on the virtual pad for `ms` milliseconds (the menu's PS button).
 * 0 if the slot has no live virtual pad. */
int  vpad_press_ps(int slot, int ms);
void vpad_remove(int slot);

/* The user's own DualSense, read only. Returns 0 if it cannot be read;
 * vpad_physical_error() then says why (a libScePad error, or 0xFFFF0001 not
 * ready, 0xFFFF0002 no pad connected, 0xFFFF0003 the system has the input, 0xFFFF0004
 * nobody is signed in; 0x809B0081 the user chosen is not signed in). */
int     vpad_read_physical(uint32_t *buttons);
int32_t vpad_physical_error(void);
void    vpad_close_physical(void);      /* closes the pad AnyPad PS5 opened, if it did */

/* Gives the process its original credentials back, and raises them again.
 * They are raised for virtual pads and put back, if the Bluetooth controller
 * turns out not to answer while they are raised. Both check what they did. */
int  vpad_creds_restore(void);
int  vpad_creds_raise(void);

void notify(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Which signed-in user gets the pads (see ps5_vpad.c); `want` is NULL, "other"
 * or a hexadecimal user id. -1 if nobody is signed in. */
int32_t vpad_choose_user(const int32_t login[4], int32_t init, int32_t fore, const char *want);

#endif
