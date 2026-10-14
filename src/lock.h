/* One running copy of AnyPad PS5 at a time.
 *
 * The lock is a file holding the owner's process id and the console's boot
 * time. It lives on persistent storage, so after a reboot or a crash it is
 * still there; a lock from another boot, or whose process is gone, is stale
 * and is taken over. (Process ids start again at every boot, so the id alone
 * says nothing: a stale lock once looked like a running copy.) */
#ifndef ANYPAD_LOCK_H
#define ANYPAD_LOCK_H

/* Returns 1 with the lock held, 0 if another copy is running or the file
 * cannot be created. */
int  lock_take(const char *path);
void lock_release(void);

/* The boot time of this machine, in seconds since 1970; 0 if unknown. */
long lock_boot_time(void);

#endif
