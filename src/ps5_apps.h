/* Registers (or unregisters) the AnyPad entry in "Contenido multimedia" with
 * the console, the way Payload Manager does for its own. */
#ifndef ANYPAD_PS5_APPS_H
#define ANYPAD_PS5_APPS_H

/* Writes the files if needed and, when they changed or the system does not
 * know the entry yet, asks the system to install it. 0 on success. */
int apps_install_launcher(void);

/* Asks the system to remove the entry and deletes the files. 0 on success. */
int apps_remove_launcher(void);

#endif
