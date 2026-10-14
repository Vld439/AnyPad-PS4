/* The controller state every profile reduces its reports to, and what the
 * console may send back. Buttons use the bit layout of ScePadData.buttons,
 * so the PS5 output needs no remapping. */
#ifndef PH_PAD_H
#define PH_PAD_H

#include <stdint.h>

enum {
    PAD_CREATE   = 0x00000001u,     /* Share / Create / View */
    PAD_L3       = 0x00000002u,
    PAD_R3       = 0x00000004u,
    PAD_OPTIONS  = 0x00000008u,
    PAD_UP       = 0x00000010u,
    PAD_RIGHT    = 0x00000020u,
    PAD_DOWN     = 0x00000040u,
    PAD_LEFT     = 0x00000080u,
    PAD_L2       = 0x00000100u,
    PAD_R2       = 0x00000200u,
    PAD_L1       = 0x00000400u,
    PAD_R1       = 0x00000800u,
    PAD_TRIANGLE = 0x00001000u,
    PAD_CIRCLE   = 0x00002000u,
    PAD_CROSS    = 0x00004000u,
    PAD_SQUARE   = 0x00008000u,
    PAD_PS       = 0x00010000u,
    PAD_TOUCHPAD = 0x00100000u,
};

typedef struct {
    uint32_t buttons;
    uint8_t  lx, ly, rx, ry;        /* 0..255, 128 centred, y grows downwards */
    uint8_t  l2, r2;                /* 0..255 */

    uint8_t  has_motion;            /* raw sensor counts, as the pad sends them */
    int16_t  gyro[3], accel[3];

    uint8_t  has_touch;
    struct { uint8_t down; uint16_t x, y; } touch[2];

    uint8_t  has_battery;
    uint8_t  battery_pct, charging;
} pad_state;

/* What the console asks of the controller. */
typedef struct {
    uint8_t strong, weak;           /* rumble motors, 0..255 */
    uint8_t r, g, b;                /* light bar, where there is one */
    uint8_t player;                 /* 1-based player number */
} pad_output;

typedef struct {
    uint16_t vid, pid;
    const char *profile;
    unsigned char addr[6];
} pad_info;

static inline void pad_state_reset(pad_state *s)
{
    *s = (pad_state){ 0 };
    s->lx = s->ly = s->rx = s->ry = 128;
}

#endif
