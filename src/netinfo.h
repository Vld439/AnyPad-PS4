#ifndef ANYPAD_NETINFO_H
#define ANYPAD_NETINFO_H

/* The address this machine uses to reach the local network, as text
 * ("<console-ip>"). Nothing is sent: a UDP socket is connected to an address
 * reserved for documentation, only to ask which interface would be used.
 * Returns 0 and writes "127.0.0.1" if there is no route. */
int local_ip(char out[16]);

#endif
