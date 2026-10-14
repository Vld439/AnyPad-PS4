#include "hotkey.h"

void hotkey_init(hotkey *h, uint32_t combo, int hold_ms)
{
    h->combo = combo;
    h->hold_ms = hold_ms;
    h->since = 0;
    h->fired_at = -HOTKEY_COOLDOWN_MS;
    h->armed = 1;
}

int hotkey_update(hotkey *h, int readable, uint32_t buttons, long now)
{
    if (!readable || (buttons & h->combo) != h->combo) {
        h->since = 0;
        /* Let go of the combination, even partly: ready for the next hold. */
        if (readable && (buttons & h->combo) == 0) h->armed = 1;
        if (!readable) h->armed = 0;
        return 0;
    }
    if (!h->armed) return 0;
    if (!h->since) h->since = now;
    if (now - h->since < h->hold_ms) return 0;
    if (now - h->fired_at < HOTKEY_COOLDOWN_MS) return 0;
    h->fired_at = now;
    h->armed = 0;
    h->since = 0;
    return 1;
}
