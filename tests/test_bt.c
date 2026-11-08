/* DualSense over Bluetooth: the reports /dev/bt hands back, the output report
 * the pad expects, and the two input reports it sends. The bytes follow the
 * layouts documented in bt_proto.c and profiles.c; they check this code
 * against those layouts, not against real hardware. */
#include "../src/bt_proto.h"
#include "../src/crc32.h"
#include "../src/profiles.h"
#include "../src/util.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void test_event(void)
{
    unsigned char ev[BT_EVENT_LEN] = { 0 };
    uint64_t addr = 0;
    int i;

    printf("device events\n");
    ev[0] = 0x19;
    for (i = 0; i < 8; i++) ev[8 + i] = (unsigned char)(0x11 * (i + 1));
    CHECK(bt_event_address(ev, &addr) == 1);
    CHECK(addr == 0x8877665544332211ULL);

    ev[0] = 0x17;
    CHECK(bt_event_address(ev, &addr) == 0);
    ev[0] = 0x21;
    CHECK(bt_event_address(ev, &addr) == 0);
    ev[0] = 0x20;
    CHECK(bt_event_address(ev, &addr) == 1);
}

static void test_unwrap(void)
{
    unsigned char buf[BT_REPORT_MAX] = { 0 };
    const uint8_t *rep = NULL;

    printf("report wrapper\n");
    buf[0] = 41;                        /* length of what follows */
    buf[1] = 0x1F;
    CHECK(bt_report_unwrap(buf, 43, &rep) == 41 && rep == buf + 2);

    CHECK(bt_report_unwrap(buf, 42, &rep) == 0);    /* length does not add up */
    buf[1] = 0x20;
    CHECK(bt_report_unwrap(buf, 43, &rep) == 0);    /* not the marker */
    buf[1] = 0x1F;
    CHECK(bt_report_unwrap(buf, 2, &rep) == 0);     /* no report */
    CHECK(bt_report_unwrap(buf, BT_REPORT_MAX + 1, &rep) == 0);
}

static void test_input_ok(void)
{
    unsigned char rep[78] = { 0 };

    printf("input reports\n");
    rep[0] = 0x01;
    CHECK(bt_input_ok(rep, 10) == 1);               /* short: no CRC to check */
    CHECK(bt_input_ok(rep, 11) == 0);

    rep[0] = 0x31;
    crc32_seal_output(rep, 78);                     /* 0xA2... */
    rep[74] ^= 0xFF;                                /* but input uses 0xA1 */
    CHECK(bt_input_ok(rep, 78) == 0);

    put32(rep + 74, crc32_hid(0xA1, rep, 74));
    CHECK(bt_input_ok(rep, 78) == 1);
    rep[5] ^= 0x01;
    CHECK(bt_input_ok(rep, 78) == 0);
    CHECK(bt_input_ok(rep, 77) == 0);
}

static void test_output(void)
{
    bt_out o;
    pad_output out;
    unsigned char buf[78];
    uint32_t want;

    printf("output report\n");
    memset(&o, 0, sizeof o);
    memset(&out, 0, sizeof out);
    out.player = 2;
    out.strong = 200;
    out.weak = 10;
    out.r = 1; out.g = 2; out.b = 3;

    CHECK(bt_output_build(&o, &out, buf, 77) == 0);

    /* The first report only ends the pairing animation. */
    CHECK(bt_output_build(&o, &out, buf, sizeof buf) == 78);
    CHECK(buf[0] == 0x31 && buf[1] == 0x00 && buf[2] == 0x10);
    CHECK(buf[41] == 0x02 && buf[44] == 0x02);
    CHECK(buf[5] == 0 && buf[6] == 0);

    /* From then on: motors, player LEDs and light bar, sequence nibble. */
    CHECK(bt_output_build(&o, &out, buf, sizeof buf) == 78);
    CHECK(buf[1] == 0x10 && buf[3] == 0x0F && buf[4] == 0x14);
    CHECK(buf[5] == 10 && buf[6] == 200);
    CHECK(buf[45] == 0x02 && buf[46] == 0x0A);
    CHECK(buf[47] == 1 && buf[48] == 2 && buf[49] == 3);

    want = (uint32_t)buf[74] | (uint32_t)buf[75] << 8 |
           (uint32_t)buf[76] << 16 | (uint32_t)buf[77] << 24;
    CHECK(want == crc32_hid(0xA2, buf, 74));

    /* Every player number has a pattern, 1 and 0 the centre LED. */
    out.player = 0;
    bt_output_build(&o, &out, buf, sizeof buf);
    CHECK(buf[46] == 0x04);
    out.player = 4;
    bt_output_build(&o, &out, buf, sizeof buf);
    CHECK(buf[46] == 0x1B && buf[1] == 0x30);
}

static void test_ds5_bt(void)
{
    unsigned char r[78] = { 0 };
    pad_state st;

    printf("DualSense over Bluetooth\n");

    /* Short report: 10 bytes, one button byte fewer than the USB form. */
    r[0] = 0x01;
    r[1] = 0x10; r[2] = 0x20; r[3] = 0x30; r[4] = 0x40;     /* sticks */
    r[5] = 0x04 | 0x20;                                     /* hat south, cross */
    r[6] = 0x02;                                            /* R1 */
    r[7] = 0x02;                                            /* touchpad */
    r[8] = 0x30;                                            /* L2 */
    r[9] = 0x40;                                            /* R2 */
    pad_state_reset(&st);
    CHECK(ds5_bt_parse(r, 10, &st) == PARSE_OK);
    CHECK(st.lx == 0x10 && st.ly == 0x20 && st.rx == 0x30 && st.ry == 0x40);
    CHECK(st.l2 == 0x30 && st.r2 == 0x40);
    CHECK((st.buttons & PAD_DOWN) && !(st.buttons & PAD_LEFT));
    CHECK(st.buttons & PAD_CROSS);
    CHECK(st.buttons & PAD_R1);
    CHECK(st.buttons & PAD_TOUCHPAD);
    CHECK((st.buttons & PAD_L2) && (st.buttons & PAD_R2));
    CHECK(!st.has_motion);

    /* Full report: the USB 0x31 layout, CRC checked by the caller. */
    memset(r, 0, sizeof r);
    r[0] = 0x31;
    r[1] = 0x05;
    r[2] = 0x11; r[3] = 0x22; r[4] = 0x33; r[5] = 0x44;
    r[6] = 0x40; r[7] = 0x50;
    r[9] = 0x02 | 0x10;                                     /* hat east, square */
    r[10] = 0x01 | 0x10;                                    /* L1, create */
    r[11] = 0x01;                                           /* PS */
    put16(r + 17, 0x0102); put16(r + 21, 0x0304);
    put16(r + 23, 0x0506); put16(r + 27, 0x0708);
    r[34] = 0x02;                                           /* finger 2 down */
    put16(r + 35, 0x0123);
    r[37] = 0x45;
    r[54] = 0x15;                                           /* level 5, charging */
    pad_state_reset(&st);
    CHECK(ds5_bt_parse(r, 78, &st) == PARSE_FULL);
    CHECK(st.lx == 0x11 && st.rx == 0x33);
    CHECK(st.buttons & PAD_RIGHT);
    CHECK(st.buttons & PAD_SQUARE);
    CHECK(st.buttons & PAD_L1);
    CHECK(st.buttons & PAD_CREATE);
    CHECK(st.buttons & PAD_PS);
    CHECK(st.has_motion && st.gyro[0] == 0x0102 && st.accel[2] == 0x0708);
    CHECK(st.has_touch && st.touch[0].down && st.touch[0].id == 2);
    CHECK(st.touch[0].x == 0x0123 && st.touch[0].y == 0x450);
    CHECK(st.has_battery && st.battery_pct == 55 && st.charging == 1);

    CHECK(ds5_bt_parse(r, 77, &st) == PARSE_NONE);
    r[0] = 0x05;
    CHECK(ds5_bt_parse(r, 78, &st) == PARSE_NONE);
}

int main(void)
{
    test_event();
    test_unwrap();
    test_input_ok();
    test_output();
    test_ds5_bt();
    printf(g_fail ? "%d check(s) failed\n" : "all Bluetooth checks passed\n", g_fail);
    return g_fail != 0;
}
