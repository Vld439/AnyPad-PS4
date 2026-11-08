/* DualSense over the console's own Bluetooth. The driver protocol, the kernel
 * allowlist patch and the 0x31 output report come from OneFamily
 * (github.com/ChiricoG/OneFamily, MIT; see BT-PLAN.md), ported to AnyPad's
 * virtual pads. */

#include "bt_proto.h"
#include "crc32.h"
#include "profiles.h"

#include <string.h>

/* One event from /dev/bt, as the driver fills it: a type, and for the ones
 * that name a device, its 64-bit kernel address at [8..15]. */
int bt_event_address(const uint8_t *ev, uint64_t *address)
{
    uint64_t a = 0;
    int i;

    if (ev[0] < BT_EVENT_FIRST || ev[0] > BT_EVENT_LAST) return 0;
    for (i = 0; i < 8; i++) a |= (uint64_t)ev[8 + i] << (8 * i);
    if (address) *address = a;
    return 1;
}

/* The driver hands back the report with its own header: the length of what
 * follows, a 0x1F marker, then the HID report (id first). */
int bt_report_unwrap(const uint8_t *buf, int n, const uint8_t **rep)
{
    if (n < 3 || n > BT_REPORT_MAX) return 0;
    if (buf[0] + 2 != n) return 0;
    if (buf[1] != 0x1F) return 0;
    if (rep) *rep = buf + 2;
    return n - 2;
}

/* Over Bluetooth every full report carries its CRC over the HID header byte
 * 0xA1 and the report, in its last four bytes. */
int bt_input_ok(const uint8_t *rep, int len)
{
    uint32_t want;

    if (len == 10 && rep[0] == 0x01) return 1;
    if (len != 78 || rep[0] != 0x31) return 0;

    want = (uint32_t)rep[74] | (uint32_t)rep[75] << 8 |
           (uint32_t)rep[76] << 16 | (uint32_t)rep[77] << 24;
    return crc32_hid(0xA1, rep, 74) == want;
}

/* DualSense output report 0x31 over Bluetooth, 78 bytes: the USB report plus
 * a byte after the sequence and the CRC at the end. Offsets (id at [0]):
 *   [1]   sequence nibble, which the pad watches for changes
 *   [2]   0x10 (tag)
 *   [3]   motors and trigger effects enabled      [4] lights enabled
 *   [5] weak motor  [6] strong motor
 *   [45] light bar brightness  [46] player LEDs  [47..49] light bar RGB */
int bt_output_build(bt_out *o, const pad_output *out, uint8_t *buf, int max)
{
    static const uint8_t player_leds[5] = { 0x04, 0x04, 0x0A, 0x15, 0x1B };

    if (max < 78) return 0;
    memset(buf, 0, 78);
    buf[0] = 0x31;
    buf[1] = (uint8_t)((o->seq & 0x0F) << 4);
    o->seq = (uint8_t)((o->seq + 1) & 0x0F);
    buf[2] = 0x10;

    if (!o->started) {
        /* Ends the blue pairing animation; the pad starts sending 0x31. */
        buf[41] = 0x02;
        buf[44] = 0x02;
        o->started = 1;
    } else {
        buf[3] = 0x03 | 0x0C;           /* rumble, both trigger effects */
        buf[4] = 0x14;                  /* light bar, player LEDs */
        buf[5] = out->weak;
        buf[6] = out->strong;
        buf[45] = 0x02;                 /* brightness */
        buf[46] = player_leds[out->player < 5 ? out->player : 4];
        buf[47] = out->r;
        buf[48] = out->g;
        buf[49] = out->b;
    }

    crc32_seal_output(buf, 78);
    return 78;
}

/* ---- families ------------------------------------------------------------- */

/* The DualSense (and the Edge, which the driver still refuses to register). */
static const bt_family k_sony = {
    "sony", "DualSense (Bluetooth)", 2, { { 0x054C, 0x0CE6 }, { 0x054C, 0x0DF2 } }, 1,
    bt_input_ok, ds5_bt_parse,
};

/* Discovery families: they ask the driver for one other pad's id and accept
 * no report at all, so bt_pad.c writes what arrives to the log instead of
 * guessing at a layout. Nothing is parsed and no pad is created; they are how
 * a family for another controller gets written, from real traffic.
 * One id each: one moving part at a time. */
static int watch_accept(const uint8_t *rep, int len)
{
    (void)rep;
    (void)len;
    return 0;
}

/* The Xbox pads answer to a different product id per model and firmware, so
 * the family asks for the usual ones at once; the ones not connected are
 * refused with ENOENT and ignored. Discovery only for now: whatever arrives
 * goes to the log, unparsed. */
static const bt_family k_xbox = {
    "xbox", "Xbox (Bluetooth)", 6,
    { { 0x045E, 0x0B13 }, { 0x045E, 0x0B20 }, { 0x045E, 0x02E0 },
      { 0x045E, 0x02FD }, { 0x045E, 0x0B05 }, { 0x045E, 0x0B0A } },
    0, watch_accept, ds5_bt_parse,
};

/* The Switch Pro Controller over the console's radio sends its standard HID
 * report 0x3F (49 bytes), which AnyPad's switch profile already reads: the
 * same one the USB pads send. No output report is sent (there is no game
 * output yet), so the link is kept by the console's own stack. */
static uint8_t g_switch_ctx[PROFILE_CTX_MAX];    /* zeroed: 0x3F needs no calibration */

static int switch_accept(const uint8_t *rep, int len)
{
    return len >= 12 && (rep[0] == 0x3F || rep[0] == 0x30);
}

static int switch_parse(const uint8_t *rep, int len, pad_state *st)
{
    const pad_profile *p = profile_find(0x057E, 0x2009);
    int r;
    uint32_t swap;

    if (!p || !p->parse) return PARSE_NONE;
    r = p->parse(g_switch_ctx, rep, len, st);
    if (r == PARSE_NONE) return r;
    /* Nintendo's labels sit where a PlayStation pad has them rotated: A is
     * the button people press to confirm, but it is the right one. Swap the
     * pairs so A is the cross and B the circle, as a PS4 game expects. */
    swap = 0;
    if (st->buttons & PAD_CROSS) swap |= PAD_CIRCLE;
    if (st->buttons & PAD_CIRCLE) swap |= PAD_CROSS;
    if (st->buttons & PAD_SQUARE) swap |= PAD_TRIANGLE;
    if (st->buttons & PAD_TRIANGLE) swap |= PAD_SQUARE;
    st->buttons = (st->buttons & ~(uint32_t)(PAD_CROSS | PAD_CIRCLE | PAD_SQUARE | PAD_TRIANGLE)) | swap;
    return r;
}

static const bt_family k_switch = {
    "switch", "Switch Pro (Bluetooth)", 1, { { 0x057E, 0x2009 } }, 0, switch_accept, switch_parse,
};

static const bt_family *const k_families[] = { &k_sony, &k_xbox, &k_switch };

const bt_family *bt_family_default(void)
{
    return &k_sony;
}

const bt_family *bt_family_find(const char *name)
{
    size_t i;

    if (!name || !name[0]) return NULL;
    for (i = 0; i < sizeof k_families / sizeof k_families[0]; i++)
        if (strcmp(k_families[i]->name, name) == 0) return k_families[i];
    return NULL;
}
