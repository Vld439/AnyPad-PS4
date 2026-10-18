/* A controller over the local network, the way Control4Free does it: a phone
 * or PC on the same network opens a page, its browser reads a real gamepad
 * (the Gamepad API — a pad paired to the phone by Bluetooth, or plugged into
 * it) and sends the pad's state here over a WebSocket. Each client becomes a
 * virtual pad on its own slot, fed through the same path the USB pads use.
 *
 * The console never sees Bluetooth or the client device: it only sees the
 * virtual DualShock 4. The phone does the Bluetooth, so this gives wireless
 * pads without touching the console's own radio or its controller stack.
 *
 * Its own thread and port (NETPAD_PORT); it does not touch web.c's loop.
 */
#ifndef PH_NETPAD_H
#define PH_NETPAD_H

#define NETPAD_PORT 8096

/* Starts the listener. Returns 1 if it is up, 0 if the port could not be
 * taken (another instance, or the system). Can be called again after
 * netpad_stop (when the network changed under it). */
int  netpad_start(int port);

/* Stops the listener, drops every client and its virtual pad, joins the
 * threads. Safe to call when it never started. */
void netpad_stop(void);

/* Is it still serving (listening, and taking a connection to this machine's
 * own address)? 0 if it never started or was stopped. */
int  netpad_alive(void);

/* How many clients are driving a pad right now (for the menu/status). */
int  netpad_clients(void);

#endif
