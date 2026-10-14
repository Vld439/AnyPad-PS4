/* The console's power state, read from the
 * SceSystemStateMgrInfo event flag, whose low 16 bits are 1000 when
 * working, 300 while going to rest mode and 500 in it. */
#ifndef PH_PS5_POWER_H
#define PH_PS5_POWER_H

enum { POWER_UNKNOWN, POWER_WORKING, POWER_GOING_TO_REST, POWER_IN_REST };

int power_state(void);

#endif
