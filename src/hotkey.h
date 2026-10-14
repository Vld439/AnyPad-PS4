/* A button combination held long enough, read from the user's own pad.
 *
 * Fires once per hold, then not again until the buttons have been let go
 * and a cooldown has passed. A pad that cannot be read resets the hold, so
 * a combination is never "completed" by a pad that stopped answering. */
#ifndef ANYPAD_HOTKEY_H
#define ANYPAD_HOTKEY_H

#include <stdint.h>

#define HOTKEY_COOLDOWN_MS 5000

typedef struct {
    uint32_t combo;
    int hold_ms;
    long since;             /* when the whole combination was first held; 0: not held */
    long fired_at;          /* last firing; -HOTKEY_COOLDOWN_MS: never */
    int armed;              /* 0 after firing, until the buttons are let go */
} hotkey;

void hotkey_init(hotkey *h, uint32_t combo, int hold_ms);

/* Feeds one reading. Returns 1 when the combination has just been held for
 * hold_ms. `readable` is 0 if the pad could not be read this time. */
int  hotkey_update(hotkey *h, int readable, uint32_t buttons, long now);

#endif
