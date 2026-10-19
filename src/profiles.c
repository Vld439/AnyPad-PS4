#include "profiles.h"
#include "crc32.h"
#include "log.h"
#include "util.h"

#include <string.h>

/* Trigger travel past which L2/R2 also count as pressed buttons. */
#define TRIGGER_PRESS 0x20

/* ---- shared helpers ------------------------------------------------------ */

/* A hat switch: 0 north, clockwise to 7 north-west; anything else is centred. */
static uint32_t hat_buttons(unsigned hat)
{
    static const uint32_t dirs[8] = {
        PAD_UP, PAD_UP | PAD_RIGHT, PAD_RIGHT, PAD_DOWN | PAD_RIGHT,
        PAD_DOWN, PAD_DOWN | PAD_LEFT, PAD_LEFT, PAD_UP | PAD_LEFT,
    };
    return hat < 8 ? dirs[hat] : 0;
}

static uint32_t trigger_buttons(uint8_t l2, uint8_t r2)
{
    return (l2 > TRIGGER_PRESS ? PAD_L2 : 0) | (r2 > TRIGGER_PRESS ? PAD_R2 : 0);
}

/* The three button bytes DualShock 4 and DualSense share:
 *   b0: hat (low nibble), square, cross, circle, triangle
 *   b1: L1, R1, L2, R2, share/create, options, L3, R3
 *   b2: PS, touchpad click, (DualSense) mute */
static uint32_t sony_buttons(const uint8_t *b)
{
    static const uint32_t b0[4] = { PAD_SQUARE, PAD_CROSS, PAD_CIRCLE, PAD_TRIANGLE };
    static const uint32_t b1[8] = { PAD_L1, PAD_R1, PAD_L2, PAD_R2,
                                    PAD_CREATE, PAD_OPTIONS, PAD_L3, PAD_R3 };
    uint32_t out = hat_buttons(b[0] & 0x0F);
    int i;

    for (i = 0; i < 4; i++) if (b[0] & (0x10 << i)) out |= b0[i];
    for (i = 0; i < 8; i++) if (b[1] & (1 << i)) out |= b1[i];
    if (b[2] & 0x01) out |= PAD_PS;
    if (b[2] & 0x02) out |= PAD_TOUCHPAD;
    return out;
}

/* A touch point: contact byte (bit 7 set = no finger), then 12-bit x and y
 * packed in three bytes. */
static void sony_touch(const uint8_t *p, pad_state *st, int i)
{
    st->touch[i].down = !(p[0] & 0x80);
    st->touch[i].id = p[0] & 0x7F;
    st->touch[i].x = (uint16_t)(p[1] | (p[2] & 0x0F) << 8);
    st->touch[i].y = (uint16_t)(p[2] >> 4 | p[3] << 4);
}

static void read_motion(const uint8_t *gyro, const uint8_t *accel, pad_state *st)
{
    int i;

    for (i = 0; i < 3; i++) {
        st->gyro[i] = les16(gyro + 2 * i);
        st->accel[i] = les16(accel + 2 * i);
    }
    st->has_motion = 1;
}

static void sony_basic(const uint8_t *sticks, const uint8_t *buttons,
                       uint8_t l2, uint8_t r2, pad_state *st)
{
    st->lx = sticks[0];
    st->ly = sticks[1];
    st->rx = sticks[2];
    st->ry = sticks[3];
    st->l2 = l2;
    st->r2 = r2;
    st->buttons = sony_buttons(buttons) | trigger_buttons(l2, r2);
}

/* ---- DualShock 4 ----------------------------------------------------------
 *
 * Over Bluetooth it starts with the short report 0x01 and moves to the full
 * report 0x11 once it receives an output report 0x11.
 *
 *   0x01: [1..4] sticks  [5..7] buttons  [8] L2  [9] R2
 *   0x11: [1..2] flags   [3..6] sticks   [7..9] buttons  [10] L2  [11] R2
 *         [15..20] gyro  [21..26] accel  [32] battery (low nibble level,
 *         bit 4 cable)   [35] touch report count, [36] first touch report:
 *         timestamp, then two 4-byte points
 */

static const uint16_t ds4_ids[][2] = {
    { 0x054C, 0x05C4 },     /* DualShock 4 v1 */
    { 0x054C, 0x09CC },     /* DualShock 4 v2 */
    { 0x0F0D, 0x00F6 },     /* Hori Onyx */
    { 0x1532, 0x1009 },     /* Razer Raiju Ultimate */
    { 0x1532, 0x100A },     /* Razer Raiju Tournament */
    { 0x2E95, 0x7725 },     /* SCUF Vantage 2 */
    { 0x0F0D, 0x0123 },     /* HORI Wireless Controller Light (Bluetooth only) */
};

static int ds4_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    (void)ctx;
    if (r[0] == 0x01 && n >= 10) {
        sony_basic(r + 1, r + 5, r[8], r[9], st);
        return PARSE_OK;
    }
    if (r[0] != 0x11 || n < 36) return PARSE_NONE;

    sony_basic(r + 3, r + 7, r[10], r[11], st);
    read_motion(r + 15, r + 21, st);

    {
        unsigned level = r[32] & 0x0F, cable = (r[32] >> 4) & 1;
        unsigned full = cable ? 11 : 8;     /* cable reports up to 11 */
        if (level > full) level = full;
        st->battery_pct = (uint8_t)(level * 100 / full);
        st->charging = (uint8_t)(cable && level < full);
        st->has_battery = 1;
    }
    if (n >= 45 && r[35] > 0) {
        sony_touch(r + 37, st, 0);
        sony_touch(r + 41, st, 1);
        st->has_touch = 1;
    }
    return PARSE_FULL;
}

/* Output report 0x11: 73 bytes after the id, then the CRC.
 *   [1] 0xC0 | report interval  [2] 0x20  [3] 0xF3 (rumble, light bar)
 *   [4] 0x04  [6] weak motor  [7] strong motor  [8..10] light bar RGB */
static int ds4_output(void *ctx, const pad_output *o, uint8_t *b, int max)
{
    const int len = 1 + 73 + 4;

    (void)ctx;
    if (max < len) return 0;
    memset(b, 0, (size_t)len);
    b[0] = 0x11;
    b[1] = 0xC0 | 0x08;                 /* 0x08: 125 Hz input reports */
    b[2] = 0x20;
    b[3] = 0xF3;
    b[4] = 0x04;
    b[6] = o->weak;
    b[7] = o->strong;
    b[8] = o->r;
    b[9] = o->g;
    b[10] = o->b;
    crc32_seal_output(b, len);
    return len;
}

/* Waking a Sony pad up is just sending it an output report. */
static int ds4_init(void *ctx, int step, const pad_output *o, uint8_t *b, int max)
{
    return step == 0 ? ds4_output(ctx, o, b, max) : 0;
}

/* ---- DualSense --------------------------------------------------------------
 *
 * Same idea: short report 0x01 until an output report 0x31 arrives.
 *
 *   0x01: [1..4] sticks  [5..8] buttons  [9] L2  [10] R2
 *   0x31: [2..5] sticks  [6] L2  [7] R2  [9..12] buttons
 *         [17..22] gyro  [23..28] accel  [34..41] two touch points
 *         [54] battery: low nibble level 0..10, high nibble 1 charging,
 *         2 full
 */

static const uint16_t ds5_ids[][2] = {
    { 0x054C, 0x0CE6 },     /* DualSense */
    { 0x054C, 0x0DF2 },     /* DualSense Edge */
};

static int ds5_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    (void)ctx;
    if (r[0] == 0x01 && n >= 11) {
        sony_basic(r + 1, r + 5, r[9], r[10], st);
        return PARSE_OK;
    }
    if (r[0] != 0x31 || n < 55) return PARSE_NONE;

    sony_basic(r + 2, r + 9, r[6], r[7], st);
    read_motion(r + 17, r + 23, st);
    sony_touch(r + 34, st, 0);
    sony_touch(r + 38, st, 1);
    st->has_touch = 1;

    {
        unsigned level = r[54] & 0x0F, status = r[54] >> 4;
        unsigned pct = level * 10 + 5;
        st->battery_pct = (uint8_t)(pct > 100 ? 100 : pct);
        st->charging = (uint8_t)(status == 1);
        st->has_battery = 1;
    }
    return PARSE_FULL;
}

/* Output report 0x31: 73 bytes after the id, then the CRC.
 *   [1] 0x02 (tag)  [2] valid flags 0: rumble  [3] valid flags 1: lights
 *   [4] weak motor  [5] strong motor  [40] valid flags 2
 *   [43] light bar setup  [44] LED brightness  [45] player LEDs
 *   [46..48] light bar RGB */
static int ds5_output(void *ctx, const pad_output *o, uint8_t *b, int max)
{
    static const uint8_t player_leds[5] = { 0x00, 0x04, 0x0A, 0x15, 0x1B };
    const int len = 1 + 73 + 4;

    (void)ctx;
    if (max < len) return 0;
    memset(b, 0, (size_t)len);
    b[0] = 0x31;
    b[1] = 0x02;
    b[2] = 0x03;
    b[3] = 0x54;
    b[4] = o->weak;
    b[5] = o->strong;
    b[40] = 0x03;
    b[43] = 0x02;
    b[44] = 0x02;
    b[45] = 0x20 | player_leds[o->player < 5 ? o->player : 4];  /* 0x20: no fade-in */
    b[46] = o->r;
    b[47] = o->g;
    b[48] = o->b;
    crc32_seal_output(b, len);
    return len;
}

static int ds5_init(void *ctx, int step, const pad_output *o, uint8_t *b, int max)
{
    return step == 0 ? ds5_output(ctx, o, b, max) : 0;
}

/* ---- Xbox One / Series ------------------------------------------------------
 *
 * The same reports over Bluetooth Classic (older firmware) and Bluetooth LE
 * (HID over GATT, current firmware), as documented publicly:
 *
 *   0x01: [1..8] sticks, 16-bit each  [9..12] triggers, 10-bit each
 *         [13] hat, 1 north .. 8 north-west, 0 centred
 *         16 bytes (first Xbox One S firmware):
 *           [14] A B X Y LB RB View Menu  [15] L3 R3
 *         17 bytes and more:
 *           [14] A B - X Y - LB RB  [15] - - View Menu Xbox L3 R3
 *           [16] bit 0: Share on Series controllers, View on older ones
 *   0x02: [1] bit 0 Xbox button (first firmware)
 *   0x04: [1] battery: bits 0-1 level, bits 2-3 power mode, bit 4 charging
 *
 * View is the "select" of Xbox games: it becomes the touchpad click, which
 * plays that part in PlayStation games. Share becomes Create.
 */

static const uint16_t xbox_ids[][2] = {
    { 0x045E, 0x02E0 },     /* Xbox One S (Bluetooth Classic) */
    { 0x045E, 0x02FD },     /* Xbox One S (Bluetooth Classic) */
    { 0x045E, 0x0B00 },     /* Elite Series 2 (Bluetooth Classic) */
    { 0x045E, 0x0B05 },     /* Elite Series 2 (Bluetooth Classic) */
    { 0x045E, 0x0B0A },     /* Adaptive Controller (Bluetooth Classic) */
    { 0x045E, 0x0B13 },     /* Xbox Series X|S (Bluetooth LE) */
    { 0x045E, 0x0B20 },     /* Xbox One S, current firmware (Bluetooth LE) */
    { 0x045E, 0x0B21 },     /* Adaptive Controller (Bluetooth LE) */
    { 0x045E, 0x0B22 },     /* Elite Series 2 (Bluetooth LE) */
};

typedef struct {
    int has_share;
} xbox_ctx;

static void xbox_setup(void *ctx, uint16_t vid, uint16_t pid)
{
    xbox_ctx *c = ctx;

    c->has_share = vid == 0x045E && (pid == 0x0B12 || pid == 0x0B13);
}

static int xbox_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    const xbox_ctx *c = ctx;
    uint32_t b = 0;
    unsigned hat;

    if (r[0] == 0x02 && n >= 2) {
        st->buttons = (st->buttons & ~(uint32_t)PAD_PS) | (r[1] & 1 ? PAD_PS : 0);
        return PARSE_OK;
    }
    if (r[0] == 0x04 && n >= 2) {
        st->battery_pct = (uint8_t)((r[1] & 3) * 100 / 3);
        st->charging = (uint8_t)((r[1] >> 4) & 1);
        st->has_battery = 1;
        return PARSE_OK;
    }
    if (r[0] != 0x01 || n < 16) return PARSE_NONE;

    st->lx = (uint8_t)(le16(r + 1) >> 8);
    st->ly = (uint8_t)(le16(r + 3) >> 8);
    st->rx = (uint8_t)(le16(r + 5) >> 8);
    st->ry = (uint8_t)(le16(r + 7) >> 8);
    st->l2 = (uint8_t)((le16(r + 9) & 0x3FF) >> 2);
    st->r2 = (uint8_t)((le16(r + 11) & 0x3FF) >> 2);

    hat = r[13];
    b |= hat >= 1 && hat <= 8 ? hat_buttons(hat - 1) : 0;

    if (n > 16) {
        if (r[14] & 0x01) b |= PAD_CROSS;
        if (r[14] & 0x02) b |= PAD_CIRCLE;
        if (r[14] & 0x08) b |= PAD_SQUARE;
        if (r[14] & 0x10) b |= PAD_TRIANGLE;
        if (r[14] & 0x40) b |= PAD_L1;
        if (r[14] & 0x80) b |= PAD_R1;
        if (r[15] & 0x04) b |= PAD_TOUCHPAD;    /* View */
        if (r[15] & 0x08) b |= PAD_OPTIONS;     /* Menu */
        if (r[15] & 0x10) b |= PAD_PS;          /* Xbox */
        if (r[15] & 0x20) b |= PAD_L3;
        if (r[15] & 0x40) b |= PAD_R3;
        if (r[16] & 0x01) b |= c && c->has_share ? PAD_CREATE : PAD_TOUCHPAD;
    } else {
        if (r[14] & 0x01) b |= PAD_CROSS;
        if (r[14] & 0x02) b |= PAD_CIRCLE;
        if (r[14] & 0x04) b |= PAD_SQUARE;
        if (r[14] & 0x08) b |= PAD_TRIANGLE;
        if (r[14] & 0x10) b |= PAD_L1;
        if (r[14] & 0x20) b |= PAD_R1;
        if (r[14] & 0x40) b |= PAD_TOUCHPAD;    /* View */
        if (r[14] & 0x80) b |= PAD_OPTIONS;     /* Menu */
        if (r[15] & 0x01) b |= PAD_L3;
        if (r[15] & 0x02) b |= PAD_R3;
        b |= st->buttons & PAD_PS;              /* comes in report 0x02 */
    }
    st->buttons = b | trigger_buttons(st->l2, st->r2);
    return PARSE_OK;
}

/* Output report 0x03, as sent over Bluetooth: all four motors
 * enabled, trigger motors, main motors (0..100), on for 2.55 s, no delay,
 * repeated 235 times: in effect until the next report. */
static int xbox_output(void *ctx, const pad_output *o, uint8_t *b, int max)
{
    (void)ctx;
    if (max < 9) return 0;
    b[0] = 0x03;
    b[1] = 0x0F;
    b[2] = 0;
    b[3] = 0;
    b[4] = (uint8_t)(o->strong * 100 / 255);
    b[5] = (uint8_t)(o->weak * 100 / 255);
    b[6] = 0xFF;
    b[7] = 0x00;
    b[8] = 0xEB;
    return 9;
}

/* ---- Nintendo Switch Pro Controller and compatibles -------------------------
 *
 * Starts in a simple HID mode (report 0x3F) and moves to the standard full
 * report 0x30 when asked with subcommands in output report 0x01:
 *
 *   0x01: [1] packet counter (low nibble)  [2..9] rumble data
 *         [10] subcommand id  [11..] subcommand arguments
 *
 *   0x30: [1] timer  [2] battery (high nibble) and connection
 *         [3] right buttons: Y X B A SR SL R ZR
 *         [4] shared: - + RS LS Home Capture
 *         [5] left buttons: down up right left SR SL L ZL
 *         [6..8] left stick, [9..11] right stick: two 12-bit values each
 *         [13..48] three IMU samples: accel xyz, gyro xyz, 16-bit each
 *
 *   0x3F: [1] B A Y X L R ZL ZR  [2] - + LS RS Home Capture  [3] hat
 *         [4..11] sticks, 16-bit each
 *
 * Buttons are placed by position, as on the PlayStation: B (bottom) is
 * cross, A (right) circle, Y (left) square, X (top) triangle.
 */

static const uint16_t switch_ids[][2] = {
    { 0x057E, 0x2009 },     /* Switch Pro Controller, and pads emulating it */
    { 0x057E, 0x2017 },     /* SNES Online */
    { 0x057E, 0x2019 },     /* N64 Online */
    { 0x057E, 0x201A },     /* Mega Drive Online */
    { 0x057E, 0x201E },     /* Mega Drive Online (second id) */
};

/* Sticks that do not rest at 2048 (clones, worn pads) drift in the console's
 * menus. The first SWITCH_CAL_N reports after connecting are taken as the
 * resting position, if the sticks were still (little spread) and not far from
 * the middle; then that offset is subtracted. Otherwise the middle is kept. */
#define SWITCH_CAL_N      16
#define SWITCH_CAL_SPREAD 120       /* raw units the samples may move and still count as still */
#define SWITCH_CAL_MAX    1000      /* ... and how far from 2048 the rest position may be */

typedef struct {
    uint8_t counter;
    uint8_t cal_n, cal_done;
    int16_t off[4];                 /* resting offset of lx, ly, rx, ry */
    int32_t sum[4];
    int16_t lo[4], hi[4];
} switch_ctx;

/* Stick travel from centre the raw values usually reach; factory
 * calibration (SPI flash) is not read yet. */
#define SWITCH_STICK_RANGE 1600

static uint8_t switch_axis(int v, int invert)
{
    int s = (v - 2048) * 127 / SWITCH_STICK_RANGE;

    if (invert) s = -s;
    s += 128;
    return (uint8_t)(s < 0 ? 0 : s > 255 ? 255 : s);
}

static void switch_sticks(switch_ctx *c, const uint8_t *l, const uint8_t *r, pad_state *st)
{
    int raw[4], i;

    raw[0] = l[0] | (l[1] & 0x0F) << 8;
    raw[1] = l[1] >> 4 | l[2] << 4;
    raw[2] = r[0] | (r[1] & 0x0F) << 8;
    raw[3] = r[1] >> 4 | r[2] << 4;

    if (!c->cal_done) {
        for (i = 0; i < 4; i++) {
            if (c->cal_n == 0) c->lo[i] = c->hi[i] = (int16_t)raw[i];
            if (raw[i] < c->lo[i]) c->lo[i] = (int16_t)raw[i];
            if (raw[i] > c->hi[i]) c->hi[i] = (int16_t)raw[i];
            c->sum[i] += raw[i];
        }
        if (++c->cal_n >= SWITCH_CAL_N) {
            for (i = 0; i < 4; i++) {
                int avg = c->sum[i] / SWITCH_CAL_N;
                int still = c->hi[i] - c->lo[i] <= SWITCH_CAL_SPREAD;
                int near = avg - 2048 <= SWITCH_CAL_MAX && 2048 - avg <= SWITCH_CAL_MAX;
                c->off[i] = (int16_t)(still && near ? avg - 2048 : 0);
            }
            c->cal_done = 1;
            log_line("switch pad: stick rest offsets %d %d %d %d", c->off[0], c->off[1], c->off[2], c->off[3]);
        }
    }
    st->lx = switch_axis(raw[0] - c->off[0], 0);
    st->ly = switch_axis(raw[1] - c->off[1], 1);    /* up is positive */
    st->rx = switch_axis(raw[2] - c->off[2], 0);
    st->ry = switch_axis(raw[3] - c->off[3], 1);
}

static int switch_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    uint32_t b = 0;

    if (r[0] == 0x3F && n >= 12) {
        if (r[1] & 0x01) b |= PAD_CROSS;            /* B */
        if (r[1] & 0x02) b |= PAD_CIRCLE;           /* A */
        if (r[1] & 0x04) b |= PAD_SQUARE;           /* Y */
        if (r[1] & 0x08) b |= PAD_TRIANGLE;         /* X */
        if (r[1] & 0x10) b |= PAD_L1;
        if (r[1] & 0x20) b |= PAD_R1;
        if (r[1] & 0x40) b |= PAD_L2;
        if (r[1] & 0x80) b |= PAD_R2;
        if (r[2] & 0x01) b |= PAD_CREATE;
        if (r[2] & 0x02) b |= PAD_OPTIONS;
        if (r[2] & 0x04) b |= PAD_L3;
        if (r[2] & 0x08) b |= PAD_R3;
        if (r[2] & 0x10) b |= PAD_PS;
        if (r[2] & 0x20) b |= PAD_TOUCHPAD;         /* Capture */
        b |= hat_buttons(r[3]);
        st->lx = (uint8_t)(le16(r + 4) >> 8);
        st->ly = (uint8_t)(le16(r + 6) >> 8);
        st->rx = (uint8_t)(le16(r + 8) >> 8);
        st->ry = (uint8_t)(le16(r + 10) >> 8);
        st->l2 = b & PAD_L2 ? 255 : 0;
        st->r2 = b & PAD_R2 ? 255 : 0;
        st->buttons = b;
        return PARSE_OK;
    }
    if (r[0] != 0x30 || n < 12) return PARSE_NONE;

    if (r[3] & 0x01) b |= PAD_SQUARE;               /* Y */
    if (r[3] & 0x02) b |= PAD_TRIANGLE;             /* X */
    if (r[3] & 0x04) b |= PAD_CROSS;                /* B */
    if (r[3] & 0x08) b |= PAD_CIRCLE;               /* A */
    if (r[3] & 0x40) b |= PAD_R1;
    if (r[3] & 0x80) b |= PAD_R2;
    if (r[4] & 0x01) b |= PAD_CREATE;               /* - */
    if (r[4] & 0x02) b |= PAD_OPTIONS;              /* + */
    if (r[4] & 0x04) b |= PAD_R3;
    if (r[4] & 0x08) b |= PAD_L3;
    if (r[4] & 0x10) b |= PAD_PS;                   /* Home */
    if (r[4] & 0x20) b |= PAD_TOUCHPAD;             /* Capture */
    if (r[5] & 0x01) b |= PAD_DOWN;
    if (r[5] & 0x02) b |= PAD_UP;
    if (r[5] & 0x04) b |= PAD_RIGHT;
    if (r[5] & 0x08) b |= PAD_LEFT;
    if (r[5] & 0x40) b |= PAD_L1;
    if (r[5] & 0x80) b |= PAD_L2;
    switch_sticks(ctx, r + 6, r + 9, st);
    st->l2 = b & PAD_L2 ? 255 : 0;                  /* ZL/ZR are digital */
    st->r2 = b & PAD_R2 ? 255 : 0;
    st->buttons = b;

    {
        unsigned level = r[2] >> 5;                 /* 0 empty .. 4 full */
        st->battery_pct = (uint8_t)(level * 25);
        st->charging = (uint8_t)((r[2] >> 4) & 1);
        st->has_battery = 1;
    }
    if (n >= 25) {
        /* First IMU sample: accel then gyro, 16-bit each. */
        int i;
        for (i = 0; i < 3; i++) {
            st->accel[i] = les16(r + 13 + 2 * i);
            st->gyro[i] = les16(r + 19 + 2 * i);
        }
        st->has_motion = 1;
    }
    return PARSE_FULL;
}

/* An output report 0x01 carrying one subcommand, rumble neutral. */
static int switch_subcmd(switch_ctx *c, uint8_t id, const uint8_t *arg, int alen,
                         uint8_t *b, int max)
{
    static const uint8_t neutral[8] = { 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };

    if (max < 11 + alen) return 0;
    b[0] = 0x01;
    b[1] = c->counter++ & 0x0F;
    memcpy(b + 2, neutral, 8);
    b[10] = id;
    memcpy(b + 11, arg, (size_t)alen);
    return 11 + alen;
}

static int switch_init(void *ctx, int step, const pad_output *o, uint8_t *b, int max)
{
    static const uint8_t full_mode[1] = { 0x30 };
    static const uint8_t on[1] = { 0x01 };
    uint8_t leds[1];
    switch_ctx *c = ctx;

    switch (step) {
    case 0: return switch_subcmd(c, 0x03, full_mode, 1, b, max);   /* report mode */
    case 1: return switch_subcmd(c, 0x40, on, 1, b, max);          /* IMU on */
    case 2: return switch_subcmd(c, 0x48, on, 1, b, max);          /* vibration on */
    case 3:
        leds[0] = (uint8_t)((1u << ((o->player ? o->player : 1) - 1)) & 0x0F);
        return switch_subcmd(c, 0x30, leds, 1, b, max);            /* player LEDs */
    default:
        return 0;
    }
}

/* ---- a single Joy-Con, held sideways ----------------------------------------
 *
 * The same subcommands and full report 0x30 as the Pro Controller, but only
 * its own side is filled in: the left one sends r[5] (down up right left SR
 * SL L ZL) and the stick at [6..8]; the right one r[3] (Y X B A SR SL R ZR)
 * and the stick at [9..11]. Held sideways with the rail up, its buttons and
 * stick turn a quarter: the left one anticlockwise, the right one clockwise
 * (as the Switch, Linux's hid-nintendo notes and Bluepad32 use them). */

static const uint16_t joycon_ids[][2] = {
    { 0x057E, 0x2006 },     /* Joy-Con (L), and the NES Online (L) */
    { 0x057E, 0x2007 },     /* Joy-Con (R), and the NES Online (R) */
};

typedef struct {
    switch_ctx sw;                  /* first: switch_init uses it */
    int right;
} joycon_ctx;

static void joycon_setup(void *ctx, uint16_t vid, uint16_t pid)
{
    joycon_ctx *c = ctx;

    (void)vid;
    c->right = pid == 0x2007;
}

static int joycon_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    joycon_ctx *c = ctx;
    pad_state t;
    uint32_t b = 0;
    uint8_t x, y_up;

    if (r[0] != 0x30 || n < 12) return PARSE_NONE;
    memset(&t, 0, sizeof t);
    if (!c->right) {
        switch_sticks(&c->sw, r + 6, r + 6, &t);
        x = t.lx; y_up = t.ly;
        st->lx = y_up;                      /* stick down (on the pad) is right, held sideways */
        st->ly = (uint8_t)(255 - x);        /* stick right is up */
        if (r[5] & 0x08) b |= PAD_CROSS;    /* left arrow: bottom */
        if (r[5] & 0x01) b |= PAD_CIRCLE;   /* down: right */
        if (r[5] & 0x02) b |= PAD_SQUARE;   /* up: left */
        if (r[5] & 0x04) b |= PAD_TRIANGLE; /* right: top */
        if (r[5] & 0x20) b |= PAD_L1;       /* SL */
        if (r[5] & 0x10) b |= PAD_R1;       /* SR */
        if (r[5] & 0x40) b |= PAD_L2;       /* L */
        if (r[5] & 0x80) b |= PAD_R2;       /* ZL */
        if (r[4] & 0x01) b |= PAD_CREATE;   /* - */
        if (r[4] & 0x20) b |= PAD_OPTIONS;  /* Capture: the only other one */
        if (r[4] & 0x08) b |= PAD_L3;       /* stick press */
    } else {
        switch_sticks(&c->sw, r + 9, r + 9, &t);
        x = t.rx; y_up = t.ry;
        st->lx = (uint8_t)(255 - y_up);     /* stick up (on the pad) is right, held sideways */
        st->ly = x;                         /* stick left is up */
        if (r[3] & 0x08) b |= PAD_CROSS;    /* A: bottom */
        if (r[3] & 0x02) b |= PAD_CIRCLE;   /* X: right */
        if (r[3] & 0x04) b |= PAD_SQUARE;   /* B: left */
        if (r[3] & 0x01) b |= PAD_TRIANGLE; /* Y: top */
        if (r[3] & 0x20) b |= PAD_L1;       /* SL */
        if (r[3] & 0x10) b |= PAD_R1;       /* SR */
        if (r[3] & 0x40) b |= PAD_L2;       /* R */
        if (r[3] & 0x80) b |= PAD_R2;       /* ZR */
        if (r[4] & 0x02) b |= PAD_OPTIONS;  /* + */
        if (r[4] & 0x10) b |= PAD_PS;       /* Home */
        if (r[4] & 0x04) b |= PAD_L3;       /* stick press */
    }
    st->rx = st->ry = 128;
    st->l2 = b & PAD_L2 ? 255 : 0;
    st->r2 = b & PAD_R2 ? 255 : 0;
    st->buttons = b;
    {
        unsigned level = r[2] >> 5;
        st->battery_pct = (uint8_t)(level * 25);
        st->charging = (uint8_t)((r[2] >> 4) & 1);
        st->has_battery = 1;
    }
    return PARSE_FULL;
}

static int joycon_init(void *ctx, int step, const pad_output *o, uint8_t *b, int max)
{
    return switch_init(&((joycon_ctx *)ctx)->sw, step, o, b, max);
}

/* ---- Nintendo Wii U Pro Controller and Wii Remote ---------------------------
 *
 * As wiibrew and Linux's hid-wiimote describe them. Output reports (on the
 * interrupt channel): 0x11 player LEDs, 0x16 write memory, 0x12 report mode.
 * The Wii U Pro is an "extension": it is woken by writing 0x55 to 0xA400F0
 * and 0x00 to 0xA400FB, then mode 0x34 (2 core button bytes, 19 extension
 * bytes) brings, from the extension bytes:
 *   [0..7] LX RX LY RY, 12 bits each, centre 0x800, about +-1280
 *   [8]  right down L - Home + R 1      [9] ZL B Y A X ZR left up
 *   [10] 1 battery(3) usb charging L3 R3        (buttons low-active)
 * A Wii Remote without extension sends its core buttons only, read held
 * sideways (NES style): [1] left right down up +  [2] 2 1 B A - . . Home.
 * After an unrequested status report (0x20) the pad sends nothing until its
 * mode is set again. */

static const uint16_t wii_ids[][2] = {
    { 0x057E, 0x0306 },     /* Wii Remote */
    { 0x057E, 0x0330 },     /* Wii U Pro Controller (and Wii Remote Plus) */
};

typedef struct {
    int moded;                      /* a full report came: a status report now drops the mode */
} wii_ctx;

static uint8_t wii_axis(const uint8_t *d, int invert)
{
    int v = (d[0] | (d[1] & 0x0F) << 8) - 0x800;
    int s = v * 127 / 1280;

    if (invert) s = -s;
    s += 128;
    return (uint8_t)(s < 0 ? 0 : s > 255 ? 255 : s);
}

/* A Wii U Pro's extension bytes: the bits that are always 1 are, and the
 * sticks are near their range (a Wii Remote alone sends no such thing). */
static int wii_is_pro(const uint8_t *e)
{
    int i;

    if (!(e[8] & 0x01) || !(e[10] & 0x80)) return 0;
    for (i = 0; i < 8; i += 2) {
        int v = e[i] | (e[i + 1] & 0x0F) << 8;
        if (e[i + 1] & 0xF0 || v < 0x200 || v > 0xE00) return 0;
    }
    return 1;
}

static void wii_core_sideways(const uint8_t *c, pad_state *st)
{
    uint32_t b = 0;

    if (c[0] & 0x08) b |= PAD_LEFT;         /* up, on the remote held sideways */
    if (c[0] & 0x04) b |= PAD_RIGHT;        /* down */
    if (c[0] & 0x01) b |= PAD_DOWN;         /* left */
    if (c[0] & 0x02) b |= PAD_UP;           /* right */
    if (c[1] & 0x01) b |= PAD_CROSS;        /* 2 */
    if (c[1] & 0x02) b |= PAD_SQUARE;       /* 1 */
    if (c[1] & 0x08) b |= PAD_CIRCLE;       /* A */
    if (c[1] & 0x04) b |= PAD_R1;           /* B, underneath */
    if (c[1] & 0x10) b |= PAD_CREATE;       /* - */
    if (c[0] & 0x10) b |= PAD_OPTIONS;      /* + */
    if (c[1] & 0x80) b |= PAD_PS;           /* Home */
    st->lx = st->ly = st->rx = st->ry = 128;
    st->l2 = st->r2 = 0;
    st->buttons = b;
}

static int wii_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    wii_ctx *c = ctx;
    const uint8_t *e;
    uint32_t b = 0;

    if (r[0] == 0x20) return c->moded ? PARSE_REINIT : PARSE_NONE;
    if (r[0] == 0x30 && n >= 3) {
        wii_core_sideways(r + 1, st);
        return PARSE_OK;
    }
    if (r[0] != 0x34 || n < 22) return PARSE_NONE;
    c->moded = 1;
    e = r + 3;
    if (!wii_is_pro(e)) {
        wii_core_sideways(r + 1, st);
        return PARSE_FULL;
    }
    st->lx = wii_axis(e + 0, 0);
    st->rx = wii_axis(e + 2, 0);
    st->ly = wii_axis(e + 4, 1);                /* up is positive */
    st->ry = wii_axis(e + 6, 1);
    if (!(e[8] & 0x80)) b |= PAD_RIGHT;
    if (!(e[8] & 0x40)) b |= PAD_DOWN;
    if (!(e[9] & 0x02)) b |= PAD_LEFT;
    if (!(e[9] & 0x01)) b |= PAD_UP;
    if (!(e[9] & 0x40)) b |= PAD_CROSS;         /* B: bottom */
    if (!(e[9] & 0x10)) b |= PAD_CIRCLE;        /* A: right */
    if (!(e[9] & 0x20)) b |= PAD_SQUARE;        /* Y: left */
    if (!(e[9] & 0x08)) b |= PAD_TRIANGLE;      /* X: top */
    if (!(e[8] & 0x20)) b |= PAD_L1;
    if (!(e[8] & 0x02)) b |= PAD_R1;
    if (!(e[9] & 0x80)) b |= PAD_L2;            /* ZL */
    if (!(e[9] & 0x04)) b |= PAD_R2;            /* ZR */
    if (!(e[8] & 0x10)) b |= PAD_CREATE;        /* - */
    if (!(e[8] & 0x04)) b |= PAD_OPTIONS;       /* + */
    if (!(e[8] & 0x08)) b |= PAD_PS;            /* Home */
    if (!(e[10] & 0x02)) b |= PAD_L3;
    if (!(e[10] & 0x01)) b |= PAD_R3;
    st->l2 = b & PAD_L2 ? 255 : 0;
    st->r2 = b & PAD_R2 ? 255 : 0;
    st->buttons = b;
    {
        unsigned level = (e[10] >> 4) & 0x07;   /* 0 .. 4 */
        st->battery_pct = (uint8_t)(level >= 4 ? 100 : level * 25);
        st->charging = (uint8_t)!(e[10] & 0x04);
        st->has_battery = 1;
    }
    return PARSE_FULL;
}

static int wii_write(uint8_t *b, int max, uint32_t addr, uint8_t value)
{
    if (max < 22) return 0;
    memset(b, 0, 22);
    b[0] = 0x16;
    b[1] = 0x04;                                /* registers, not EEPROM */
    b[2] = (uint8_t)(addr >> 16);
    b[3] = (uint8_t)(addr >> 8);
    b[4] = (uint8_t)addr;
    b[5] = 1;
    b[6] = value;
    return 22;
}

static int wii_init(void *ctx, int step, const pad_output *o, uint8_t *b, int max)
{
    int player = o->player >= 1 && o->player <= 4 ? o->player : 1;

    (void)ctx;
    if (max < 3) return 0;
    switch (step) {
    case 0:
        b[0] = 0x11;
        b[1] = (uint8_t)(0x10 << (player - 1));    /* player LED */
        return 2;
    case 1: return wii_write(b, max, 0xA400F0, 0x55);  /* wake the extension */
    case 2: return wii_write(b, max, 0xA400FB, 0x00);
    case 3:
        b[0] = 0x12;
        b[1] = 0x04;                                /* continuous */
        b[2] = 0x34;                                /* core buttons and 19 extension bytes */
        return 3;
    default:
        return 0;
    }
}

/* ---- GameSir -------------------------------------------------------------
 * GameSir G8 (Galileo / G8+), Kaleid, T4 series, Nova, Cyclone.
 * In Xbox/XInput mode, reports follow standard Xbox layout.
 * In standard HID/DInput mode, standard 8+ byte layout.
 */
static const uint16_t gamesir_ids[][2] = {
    { 0x3537, 0x0108 },     /* GameSir G8 Galileo / G8+ */
    { 0x3537, 0x1004 },     /* GameSir T4 Kaleid */
    { 0x3537, 0x1012 },     /* GameSir Kaleid / Kaleid Flux */
    { 0x3537, 0x1097 },     /* GameSir Kaleid USB / G8 */
    { 0x3537, 0x1014 },     /* GameSir Kaleid / G8 mode */
    { 0x3537, 0x1067 },     /* GameSir G8 / Kaleid alternate PID */
    { 0x3537, 0x1001 },     /* GameSir T4 Pro */
    { 0x3537, 0x1009 },     /* GameSir Nova / Nova Lite */
    { 0x3537, 0x1011 },     /* GameSir Cyclone / Cyclone Pro */
};

static int xinput_wired_parse(const uint8_t *r, int n, pad_state *st)
{
    if (n < 14) return PARSE_NONE;
    if (r[0] == 0x00) {
        uint32_t b = 0;
        uint8_t b1 = r[2];
        uint8_t b2 = r[3];

        /* D-Pad */
        if (b1 & 0x01) b |= PAD_UP;
        if (b1 & 0x02) b |= PAD_DOWN;
        if (b1 & 0x04) b |= PAD_LEFT;
        if (b1 & 0x08) b |= PAD_RIGHT;
        if (b1 & 0x10) b |= PAD_OPTIONS;  /* Start */
        if (b1 & 0x20) b |= PAD_TOUCHPAD; /* Back */
        if (b1 & 0x40) b |= PAD_L3;
        if (b1 & 0x80) b |= PAD_R3;

        /* Buttons */
        if (b2 & 0x01) b |= PAD_L1;
        if (b2 & 0x02) b |= PAD_R1;
        if (b2 & 0x04) b |= PAD_PS;       /* Guide */
        if (b2 & 0x10) b |= PAD_CROSS;    /* A */
        if (b2 & 0x20) b |= PAD_CIRCLE;   /* B */
        if (b2 & 0x40) b |= PAD_SQUARE;   /* X */
        if (b2 & 0x80) b |= PAD_TRIANGLE; /* Y */

        st->l2 = r[4];
        st->r2 = r[5];
        st->buttons = b | trigger_buttons(st->l2, st->r2);

        /* Sticks: int16 (-32768..32767) to uint8 (0..255) */
        int16_t lx = (int16_t)(r[6] | (r[7] << 8));
        int16_t ly = (int16_t)(r[8] | (r[9] << 8));
        int16_t rx = (int16_t)(r[10] | (r[11] << 8));
        int16_t ry = (int16_t)(r[12] | (r[13] << 8));

        st->lx = (uint8_t)((lx + 32768) >> 8);
        st->ly = (uint8_t)((32767 - ly) >> 8);
        st->rx = (uint8_t)((rx + 32768) >> 8);
        st->ry = (uint8_t)((32767 - ry) >> 8);

        return PARSE_OK;
    }
    return PARSE_NONE;
}

static int gamesir_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    if (n < 8) return PARSE_NONE;

    /* GameSir standard 10-byte HID report (Report ID 0x05) */
    if (r[0] == 0x05 && n >= 10) {
        uint8_t b1 = r[1], b2 = r[2], hat = r[3];
        uint32_t b = 0;

        /* Hat switch (0..7, 8 centered) */
        if (hat < 8) b |= hat_buttons(hat);

        /* Face & Shoulder Buttons (b1) */
        if (b1 & 0x01) b |= PAD_CROSS;    /* A */
        if (b1 & 0x02) b |= PAD_CIRCLE;   /* B */
        if (b1 & 0x08) b |= PAD_SQUARE;   /* X */
        if (b1 & 0x10) b |= PAD_TRIANGLE; /* Y */
        if (b1 & 0x40) b |= PAD_L1;
        if (b1 & 0x80) b |= PAD_R1;

        /* System & Stick Clicks (b2), read off a Kaleid Flux (3537:1097):
         * 0x01 LT and 0x02 RT pressed (the analog values below say it too) */
        if (b2 & 0x04) b |= PAD_TOUCHPAD; /* View */
        if (b2 & 0x08) b |= PAD_OPTIONS;  /* Menu */
        if (b2 & 0x10) b |= PAD_PS;       /* Home */
        if (b2 & 0x20) b |= PAD_L3;
        if (b2 & 0x40) b |= PAD_R3;

        /* Sticks (0..255, center ~128) */
        st->lx = r[4];
        st->ly = r[5];
        st->rx = r[6];
        st->ry = r[7];

        /* Analog Triggers */
        st->l2 = r[8];
        st->r2 = r[9];

        st->buttons = b | trigger_buttons(st->l2, st->r2);
        return PARSE_OK;
    }

    /* Wired Xbox 360 / XInput mode (e.g. GameSir Kaleid, G8 wired) */
    if (r[0] == 0x00 && n >= 14) {
        int res = xinput_wired_parse(r, n, st);
        if (res == PARSE_OK) return PARSE_OK;
    }

    /* Switch mode */
    if (r[0] == 0x3F || r[0] == 0x30 || r[0] == 0x21) {
        return switch_parse(ctx, r, n, st);
    }

    /* DualShock 4 mode report */
    if (r[0] == 0x01 && n >= 10 && (r[5] & 0x0F) <= 8 && (r[5] & 0xF0) != 0) {
        sony_basic(r + 1, r + 5, r[8], r[9], st);
        return PARSE_OK;
    }

    /* Standard GameSir HID report */
    st->lx = r[1];
    st->ly = r[2];
    st->rx = r[3];
    st->ry = r[4];
    if (n >= 7) {
        st->l2 = r[5];
        st->r2 = r[6];
    }
    if (n >= 9) {
        uint8_t b1 = r[7], b2 = r[8];
        uint32_t b = 0;
        if (b1 & 0x01) b |= PAD_CROSS;    /* A */
        if (b1 & 0x02) b |= PAD_CIRCLE;   /* B */
        if (b1 & 0x08) b |= PAD_SQUARE;   /* X */
        if (b1 & 0x10) b |= PAD_TRIANGLE; /* Y */
        if (b1 & 0x40) b |= PAD_L1;
        if (b1 & 0x80) b |= PAD_R1;
        if (b2 & 0x01) b |= PAD_CREATE;   /* Select / View */
        if (b2 & 0x02) b |= PAD_OPTIONS;  /* Start / Menu */
        if (b2 & 0x04) b |= PAD_PS;       /* Home / PS */
        if (b2 & 0x20) b |= PAD_L3;
        if (b2 & 0x40) b |= PAD_R3;
        st->buttons = b | trigger_buttons(st->l2, st->r2);
    }
    return PARSE_OK;
}

/* ---- DualShock 3 (USB) --------------------------------------------------
 *
 * The PS4 does not take it, so it is not left to the console as Sony's other
 * pads are. Silent until asked for its feature report 0xF2 (done when it is
 * opened). Report 0x01, as Linux's hid-sony reads it:
 *   [2] Select L3 R3 Start Up Right Down Left   [3] L2 R2 L1 R1 Tri Cir Cross Sq
 *   [4] PS   [6..9] LX LY RX RY   [18] [19] L2 R2 pressure
 * Select is the touchpad, as View and Back are on the other pads. Output
 * report 0x01 (36 bytes, by SET_REPORT): rumble, then the player LEDs. */

static const uint16_t ds3_ids[][2] = {
    { 0x054C, 0x0268 },     /* DualShock 3 / Sixaxis */
};

static int ds3_parse(void *ctx, const uint8_t *r, int n, pad_state *st)
{
    uint32_t b = 0;

    (void)ctx;
    if (n < 20 || r[0] != 0x01) return PARSE_NONE;
    if (r[2] & 0x01) b |= PAD_TOUCHPAD;     /* Select */
    if (r[2] & 0x02) b |= PAD_L3;
    if (r[2] & 0x04) b |= PAD_R3;
    if (r[2] & 0x08) b |= PAD_OPTIONS;      /* Start */
    if (r[2] & 0x10) b |= PAD_UP;
    if (r[2] & 0x20) b |= PAD_RIGHT;
    if (r[2] & 0x40) b |= PAD_DOWN;
    if (r[2] & 0x80) b |= PAD_LEFT;
    if (r[3] & 0x01) b |= PAD_L2;
    if (r[3] & 0x02) b |= PAD_R2;
    if (r[3] & 0x04) b |= PAD_L1;
    if (r[3] & 0x08) b |= PAD_R1;
    if (r[3] & 0x10) b |= PAD_TRIANGLE;
    if (r[3] & 0x20) b |= PAD_CIRCLE;
    if (r[3] & 0x40) b |= PAD_CROSS;
    if (r[3] & 0x80) b |= PAD_SQUARE;
    if (r[4] & 0x01) b |= PAD_PS;
    st->buttons = b;
    st->lx = r[6];
    st->ly = r[7];
    st->rx = r[8];
    st->ry = r[9];
    st->l2 = r[18];
    st->r2 = r[19];
    return PARSE_FULL;
}

static int ds3_output(void *ctx, const pad_output *out, uint8_t *buf, int max)
{
    static const uint8_t led[5] = { 0xFF, 0x27, 0x10, 0x00, 0x32 };    /* always on */
    int i;

    (void)ctx;
    if (max < 36) return 0;
    memset(buf, 0, 36);
    buf[0] = 0x01;
    buf[2] = out->weak ? 0xFF : 0;          /* right (small) motor: duration, on */
    buf[3] = out->weak ? 1 : 0;
    buf[4] = out->strong ? 0xFF : 0;        /* left (big) motor: duration, force */
    buf[5] = out->strong;
    buf[10] = (uint8_t)(out->player >= 1 && out->player <= 4 ? 1 << out->player : 0x02);
    for (i = 0; i < 4; i++) memcpy(buf + 11 + i * 5, led, sizeof led);
    return 36;
}

/* Lights its player LED once it is awake, so it stops blinking. */
static int ds3_init(void *ctx, int step, const pad_output *out, uint8_t *buf, int max)
{
    return step == 0 ? ds3_output(ctx, out, buf, max) : 0;
}

/* ---- the table --------------------------------------------------------- */

#define IDS(t) t, (int)(sizeof t / sizeof t[0])

static const pad_profile k_profiles[] = {
    { "DualShock 4", IDS(ds4_ids),    ds4_parse,    ds4_output,  ds4_init,    1, 0, NULL, 0x05, 0 },
    { "DualSense",   IDS(ds5_ids),    ds5_parse,    ds5_output,  ds5_init,    1, 0, NULL, 0, 0 },
    { "Xbox",        IDS(xbox_ids),   xbox_parse,   xbox_output, NULL,        0, 0, xbox_setup, 0, 0 },
    { "Switch Pro",  IDS(switch_ids), switch_parse, NULL,        switch_init, 1, 0, NULL, 0, 0 },
    { "Joy-Con",     IDS(joycon_ids), joycon_parse, NULL,        joycon_init, 1, 0, joycon_setup, 0, 0 },
    { "Wii",         IDS(wii_ids),    wii_parse,    NULL,        wii_init,    1, 0, NULL, 0, 0 },
    { "GameSir",     IDS(gamesir_ids),gamesir_parse,NULL,        NULL,        0, 0, NULL, 0, 0 },
    { "DualShock 3", IDS(ds3_ids),    ds3_parse,    ds3_output,  ds3_init,    0, 0, NULL, 0xF2, 1 },
};

const pad_profile *profile_find(uint16_t vid, uint16_t pid)
{
    size_t p;
    int i;

    for (p = 0; p < sizeof k_profiles / sizeof k_profiles[0]; p++)
        for (i = 0; i < k_profiles[p].nids; i++)
            if (k_profiles[p].ids[i][0] == vid && k_profiles[p].ids[i][1] == pid)
                return &k_profiles[p];
    return NULL;
}

int profile_class_is_gamepad(uint32_t cod)
{
    unsigned major = (cod >> 8) & 0x1F, minor = (cod >> 2) & 0x0F;

    return major == 5 && (minor == 1 || minor == 2);
}
