/* Report parsing and output building, profile by profile. The report bytes
 * follow the layouts documented in profiles.c; they check this code against
 * those layouts, not against real hardware. */
#include "../src/crc32.h"
#include "../src/profiles.h"
#include "../src/util.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void test_crc(void)
{
    /* The precomputed seeds: the CRC of the two bytes that
     * start each Sony output report. */
    static const unsigned char ds4[2] = { 0xA2, 0x11 }, ds5[2] = { 0xA2, 0x31 };

    printf("crc32\n");
    CHECK(crc32_calc((const unsigned char *)"123456789", 9) == 0xCBF43926u);
    CHECK(crc32_calc(ds4, 2) == 0xB758EC66u);
    CHECK(crc32_calc(ds5, 2) == 0x8C36CCAEu);
}

static void test_lookup(void)
{
    printf("profile lookup\n");
    CHECK(profile_find(0x054C, 0x09CC) && strcmp(profile_find(0x054C, 0x09CC)->name, "DualShock 4") == 0);
    CHECK(profile_find(0x054C, 0x0CE6) && strcmp(profile_find(0x054C, 0x0CE6)->name, "DualSense") == 0);
    CHECK(profile_find(0x045E, 0x02FD) && strcmp(profile_find(0x045E, 0x02FD)->name, "Xbox") == 0);
    CHECK(profile_find(0x1234, 0x5678) == NULL);
    CHECK(profile_class_is_gamepad(0x002508));      /* DualShock 4, Xbox One S */
    CHECK(profile_class_is_gamepad(0x000504));      /* joystick */
    CHECK(!profile_class_is_gamepad(0x240404));     /* headset */
    CHECK(!profile_class_is_gamepad(0x002540));     /* keyboard */
}

static void test_ds4_short(void)
{
    const pad_profile *p = profile_find(0x054C, 0x05C4);
    unsigned char r[10] = { 0x01, 0x00, 0xFF, 0x80, 0x80, 0x40 | 0x02, 0x01 | 0x20, 0x01, 0x00, 0xFF };
    pad_state st;

    printf("DualShock 4 short report\n");
    pad_state_reset(&st);
    CHECK(p->parse(NULL, r, sizeof r, &st));
    CHECK(st.lx == 0x00 && st.ly == 0xFF);
    CHECK(st.buttons & 0x2000);                     /* circle */
    CHECK(st.buttons & 0x20);                       /* hat east: right */
    CHECK(st.buttons & 0x400);                      /* L1 */
    CHECK(st.buttons & 0x8);                        /* options */
    CHECK(st.buttons & 0x10000);                    /* PS */
    CHECK(st.r2 == 0xFF && (st.buttons & 0x200));
    CHECK(!p->parse(NULL, r, 5, &st));
}

static void test_ds5(void)
{
    const pad_profile *p = profile_find(0x054C, 0x0CE6);
    unsigned char r[78];
    unsigned char out[128];
    pad_output o = { 10, 20, 0x11, 0x22, 0x33, 2 };
    pad_state st;
    int n;

    printf("DualSense full report and output\n");
    memset(r, 0, sizeof r);
    r[0] = 0x31;
    r[2] = 0x10; r[3] = 0x20; r[4] = 0x30; r[5] = 0x40;
    r[6] = 0x50; r[7] = 0x00;
    r[9] = 0x80 | 0x07;                     /* triangle, hat north-west */
    r[10] = 0x40 | 0x80;                    /* L3, R3 */
    r[11] = 0x02;                           /* touchpad click */
    r[17] = 0x34; r[18] = 0x12;             /* gyro x = 0x1234 */
    r[23] = 0xFF; r[24] = 0xFF;             /* accel x = -1 */
    r[34] = 0x05; r[35] = 0x80; r[36] = 0x37; r[37] = 0x12;   /* finger 5 down at (0x780, 0x123) */
    r[38] = 0x80;                           /* second finger up */
    r[54] = 0x18;                           /* charging, level 8 */
    pad_state_reset(&st);
    CHECK(p->parse(NULL, r, sizeof r, &st));
    CHECK(st.lx == 0x10 && st.ly == 0x20 && st.rx == 0x30 && st.ry == 0x40);
    CHECK(st.l2 == 0x50 && (st.buttons & 0x100));
    CHECK((st.buttons & 0x1000) && (st.buttons & 0x10) && (st.buttons & 0x80));
    CHECK((st.buttons & 0x2) && (st.buttons & 0x4) && (st.buttons & 0x100000));
    CHECK(st.has_motion && st.gyro[0] == 0x1234 && st.accel[0] == -1);
    CHECK(st.has_touch && st.touch[0].down && st.touch[0].x == 0x780 && st.touch[0].y == 0x123);
    CHECK(st.touch[0].id == 5);
    CHECK(!st.touch[1].down);
    CHECK(st.battery_pct == 85 && st.charging);

    n = p->build_output(NULL, &o, out, sizeof out);
    CHECK(n == 78 && out[0] == 0x31);
    CHECK(out[4] == 20 && out[5] == 10);
    CHECK(out[46] == 0x11 && out[47] == 0x22 && out[48] == 0x33);
    {
        unsigned char framed[79];
        framed[0] = 0xA2;
        memcpy(framed + 1, out, 78);
        CHECK(crc32_calc(framed, 75) == ((uint32_t)framed[75] | (uint32_t)framed[76] << 8 |
                                         (uint32_t)framed[77] << 16 | (uint32_t)framed[78] << 24));
    }
}

static void test_xbox(void)
{
    const pad_profile *p = profile_find(0x045E, 0x02E0);
    const pad_profile *series = profile_find(0x045E, 0x0B13);
    unsigned char ctx[PROFILE_CTX_MAX];
    unsigned char r[17];
    unsigned char out[16];
    pad_output o = { 255, 0, 0, 0, 0, 1 };
    pad_state st;

    printf("Xbox report: every firmware layout, Series share button\n");
    CHECK(series == p);
    memset(ctx, 0, sizeof ctx);
    p->setup(ctx, 0x045E, 0x02E0);
    memset(r, 0, sizeof r);
    r[0] = 0x01;
    r[1] = 0x00; r[2] = 0x00;               /* left x: 0 */
    r[3] = 0xFF; r[4] = 0xFF;               /* left y: max */
    r[5] = 0x00; r[6] = 0x80;               /* right x: centre */
    r[7] = 0x00; r[8] = 0x80;
    r[9] = 0xFF; r[10] = 0x03;              /* left trigger: full */
    r[13] = 5;                              /* hat south */
    r[14] = 0x01 | 0x10 | 0x40;             /* A, Y, LB */
    r[15] = 0x10 | 0x08 | 0x04;             /* Xbox, Menu, View */
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, 17, &st));
    CHECK(st.lx == 0 && st.ly == 0xFF && st.rx == 0x80);
    CHECK(st.l2 == 0xFF && (st.buttons & 0x100));
    CHECK(st.buttons & 0x40);                       /* down */
    CHECK((st.buttons & 0x4000) && (st.buttons & 0x1000) && (st.buttons & 0x400));
    CHECK((st.buttons & 0x10000) && (st.buttons & 0x8) && (st.buttons & 0x100000));
    CHECK(!(st.buttons & 0x1));

    r[15] = 0;                              /* fw 4.8: View in byte 16 */
    r[16] = 0x01;
    CHECK(p->parse(ctx, r, 17, &st) && (st.buttons & 0x100000) && !(st.buttons & 0x1));
    p->setup(ctx, 0x045E, 0x0B13);          /* Series: byte 16 is Share */
    CHECK(p->parse(ctx, r, 17, &st) && (st.buttons & 0x1) && !(st.buttons & 0x100000));

    r[14] = 0x04 | 0x80;                    /* first layout: X, Menu */
    r[15] = 0x02;                           /* R3 */
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, 16, &st));
    CHECK((st.buttons & 0x8000) && (st.buttons & 0x8) && (st.buttons & 0x4));
    CHECK(!(st.buttons & 0x10000));
    {
        unsigned char guide[2] = { 0x02, 0x01 };
        CHECK(p->parse(ctx, guide, 2, &st) && (st.buttons & 0x10000));
    }

    CHECK(p->build_output(NULL, &o, out, sizeof out) == 9);
    CHECK(out[0] == 0x03 && out[1] == 0x0F && out[4] == 100 && out[5] == 0);
    CHECK(out[6] == 0xFF && out[8] == 0xEB);
}

static void test_switch(void)
{
    const pad_profile *p = profile_find(0x057E, 0x2009);
    unsigned char r[49], out[64];
    unsigned char ctx[PROFILE_CTX_MAX];
    pad_output o = { 0, 0, 0, 0, 0, 2 };
    pad_state st;
    int n, step;

    memset(ctx, 0, sizeof ctx);
    printf("Switch Pro: full report, simple report, wake-up\n");
    CHECK(p && strcmp(p->name, "Switch Pro") == 0);
    memset(r, 0, sizeof r);
    r[0] = 0x30;
    r[2] = 0x90;                            /* battery 4/4, charging */
    r[3] = 0x04 | 0x80;                     /* B, ZR */
    r[4] = 0x10 | 0x01;                     /* Home, minus */
    r[5] = 0x02 | 0x40;                     /* up, L */
    /* left stick: x 2048 + 1600 (full right), y 2048 + 1600 (full up) */
    r[6] = 0x40; r[7] = 0x0E | 0x00; r[8] = 0xE4;
    {   /* x = 0xE40 = 3648, y = 0xE40: x in r6 + low nibble r7, y high nibble r7 + r8 */
        unsigned x = 3648, y = 3648;
        r[6] = (unsigned char)x; r[7] = (unsigned char)((x >> 8) | (y & 0x0F) << 4); r[8] = (unsigned char)(y >> 4);
        x = 2048; y = 2048;
        r[9] = (unsigned char)x; r[10] = (unsigned char)((x >> 8) | (y & 0x0F) << 4); r[11] = (unsigned char)(y >> 4);
    }
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, sizeof r, &st) == PARSE_FULL);
    CHECK((st.buttons & 0x4000) && (st.buttons & 0x200) && st.r2 == 255);   /* cross, R2 */
    CHECK((st.buttons & 0x10000) && (st.buttons & 0x1));                    /* PS, create */
    CHECK((st.buttons & 0x10) && (st.buttons & 0x400));                     /* up, L1 */
    CHECK(st.lx == 255 && st.ly == 1);      /* right, up (y grows downwards) */
    CHECK(st.rx == 128 && st.ry == 128);
    CHECK(st.battery_pct == 100 && st.charging);

    memset(r, 0, sizeof r);
    r[0] = 0x3F;
    r[1] = 0x02;                            /* A */
    r[3] = 8;                               /* hat centred */
    put16(r + 4, 0x8000); put16(r + 6, 0x8000); put16(r + 8, 0); put16(r + 10, 0xFFFF);
    CHECK(p->parse(ctx, r, 12, &st) == PARSE_OK);
    CHECK((st.buttons & 0x2000) && !(st.buttons & 0xF0));
    CHECK(st.lx == 0x80 && st.rx == 0 && st.ry == 0xFF);

    {   /* sticks that rest off-centre are re-centred after the first reports */
        unsigned char rr[49];
        unsigned x, y, i;

        memset(ctx, 0, sizeof ctx);
        for (i = 0; i < 20; i++) {
            memset(rr, 0, sizeof rr);
            rr[0] = 0x30;
            x = 2900; y = 2048;                                 /* left rests +852 on x */
            rr[6] = (unsigned char)x; rr[7] = (unsigned char)((x >> 8) | (y & 0x0F) << 4); rr[8] = (unsigned char)(y >> 4);
            x = 2048; y = 2048;
            rr[9] = (unsigned char)x; rr[10] = (unsigned char)((x >> 8) | (y & 0x0F) << 4); rr[11] = (unsigned char)(y >> 4);
            pad_state_reset(&st);
            CHECK(p->parse(ctx, rr, sizeof rr, &st) == PARSE_FULL);
            if (i == 0) CHECK(st.lx > 128 + 60);                /* before it is known, raw */
        }
        CHECK(st.lx == 128 && st.ly == 128 && st.rx == 128);    /* centred afterwards */
        x = 4000;                                               /* then a real push to the right */
        rr[6] = (unsigned char)x; rr[7] = (unsigned char)((x >> 8) | (2048 & 0x0F) << 4); rr[8] = (unsigned char)(2048 >> 4);
        pad_state_reset(&st);
        CHECK(p->parse(ctx, rr, sizeof rr, &st) == PARSE_FULL && st.lx > 200);

        /* a stick held away (moving) at connection is not taken as the rest position */
        memset(ctx, 0, sizeof ctx);
        for (i = 0; i < 20; i++) {
            memset(rr, 0, sizeof rr);
            rr[0] = 0x30;
            x = 1800 + (i % 2) * 1500; y = 2048;                /* swinging about */
            rr[6] = (unsigned char)x; rr[7] = (unsigned char)((x >> 8) | (y & 0x0F) << 4); rr[8] = (unsigned char)(y >> 4);
            x = 2048;
            rr[9] = (unsigned char)x; rr[10] = (unsigned char)((x >> 8) | (y & 0x0F) << 4); rr[11] = (unsigned char)(y >> 4);
            pad_state_reset(&st);
            p->parse(ctx, rr, sizeof rr, &st);
        }
        x = 2048;
        rr[6] = (unsigned char)x; rr[7] = (unsigned char)((x >> 8) | (2048 & 0x0F) << 4); rr[8] = (unsigned char)(2048 >> 4);
        pad_state_reset(&st);
        CHECK(p->parse(ctx, rr, sizeof rr, &st) == PARSE_FULL && st.lx == 128);   /* the middle stays 2048 */
    }

    memset(ctx, 0, sizeof ctx);
    for (step = 0; (n = p->init_report(ctx, step, &o, out, sizeof out)) > 0; step++) {
        CHECK(out[0] == 0x01 && out[1] == step && n == 12);
        if (step == 0) CHECK(out[10] == 0x03 && out[11] == 0x30);       /* full report mode */
        if (step == 3) CHECK(out[10] == 0x30 && out[11] == 0x02);       /* player 2 LED */
    }
    CHECK(step == 4);
}

/* A typical Bluetooth gamepad descriptor: report 1 with X, Y, Z, Rz
 * (8-bit), a 4-bit hat plus 4 bits padding, 15 buttons plus one bit
 * padding, then brake and accelerator (8-bit), and AC Home on report 2. */
static const unsigned char k_desc[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,             /* Generic Desktop, Game Pad, App */
    0x85, 0x01,                                     /* report id 1 */
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x04,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,    /* X Y Z Rz */
    0x15, 0x00, 0x25, 0x07, 0x75, 0x04, 0x95, 0x01,
    0x09, 0x39, 0x81, 0x42,                         /* hat, null state */
    0x75, 0x04, 0x95, 0x01, 0x81, 0x03,             /* padding */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0F, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x0F, 0x81, 0x02,             /* buttons 1-15 */
    0x75, 0x01, 0x95, 0x01, 0x81, 0x03,             /* padding */
    0x05, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02,
    0x09, 0xC5, 0x09, 0xC4, 0x81, 0x02,             /* brake, accelerator */
    0x85, 0x02, 0x05, 0x0C, 0x0A, 0x23, 0x02,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02,    /* AC Home */
    0x75, 0x07, 0x95, 0x01, 0x81, 0x03,
    0xC0,
};

static void test_generic(void)
{
    unsigned char ctx[PROFILE_CTX_MAX];
    const pad_profile *p = &generic_profile;
    unsigned char r[10];
    pad_state st;
    FILE *f;

    printf("generic HID: descriptor, default mapping, mapping file\n");
    CHECK(generic_setup(ctx, k_desc, sizeof k_desc, 0x1234, 0x5678, NULL));

    memset(r, 0, sizeof r);
    r[0] = 0x01;
    r[1] = 0x00; r[2] = 0xFF;               /* left: full left, full down */
    r[3] = 0x80; r[4] = 0x80;               /* right: centred */
    r[5] = 0x02;                            /* hat: east */
    r[6] = 0x01 | 0x10;                     /* buttons 1 (A) and 5 (Y) */
    r[7] = 0x10;                            /* button 13 (home) */
    r[8] = 0x00; r[9] = 0xFF;               /* brake 0, accelerator full */
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, sizeof r, &st) == PARSE_OK);
    CHECK(st.lx == 0 && st.ly == 255 && st.rx == 128 && st.ry == 128);
    CHECK(st.buttons & 0x20);               /* right */
    CHECK((st.buttons & 0x4000) && (st.buttons & 0x1000));   /* cross, triangle */
    CHECK(st.buttons & 0x10000);            /* PS */
    CHECK(st.l2 == 0 && st.r2 == 255 && (st.buttons & 0x200));
    {
        unsigned char home[2] = { 0x02, 0x01 };
        CHECK(p->parse(ctx, home, 2, &st) == PARSE_OK && (st.buttons & 0x10000));
    }

    /* A mapping file swaps buttons, moves the right stick, inverts. */
    f = fopen("build/1234_5678.map", "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs("# test\nbutton1 = circle\nbutton5 = none\nrx = rz\nry = z\ninvert ly\nbogus line\n", f);
    fclose(f);
    CHECK(generic_setup(ctx, k_desc, sizeof k_desc, 0x1234, 0x5678, "build"));
    r[3] = 0x10; r[4] = 0x20;
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, sizeof r, &st) == PARSE_OK);
    CHECK((st.buttons & 0x2000) && !(st.buttons & 0x4000) && !(st.buttons & 0x1000));
    CHECK(st.ly == 0);                      /* inverted */
    CHECK(st.rx == 0x20 && st.ry == 0x10);

    /* A built-in mapping: the NVIDIA Shield's start is button 9. */
    CHECK(generic_setup(ctx, k_desc, sizeof k_desc, 0x0955, 0x7214, NULL));
    memset(r + 5, 0, 3);
    r[5] = 0x08;                            /* hat centred */
    r[7] = 0x01;                            /* button 9 */
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, sizeof r, &st) == PARSE_OK);
    CHECK((st.buttons & 0x8) && !(st.buttons & 0x100));     /* options, not L2 */

    {   /* A report count of four billion: refused at once. */
        static const unsigned char huge[] = {
            0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x75, 0x01,
            0x97, 0xFF, 0xFF, 0xFF, 0xFF, 0x81, 0x02,
        };
        CHECK(!generic_setup(ctx, huge, sizeof huge, 1, 2, NULL));
    }
    {
        static const unsigned char junk[] = { 0x05, 0x01, 0x09 };
        CHECK(!generic_setup(ctx, junk, sizeof junk, 1, 2, NULL));
    }
}

static void test_ds3(void)
{
    const pad_profile *p = profile_find(0x054C, 0x0268);
    unsigned char ctx[PROFILE_CTX_MAX], r[49], out[64];
    pad_output o = { 0, 0, 0, 0, 0, 3 };
    pad_state st;
    int n;

    printf("DualShock 3 (USB): report 0x01, Select as SHARE, LEDs by SET_REPORT\n");
    CHECK(p && strcmp(p->name, "DualShock 3") == 0);
    CHECK(p && p->wake_feature == 0xF2 && p->out_control == 1);
    if (!p) return;
    memset(ctx, 0, sizeof ctx);
    memset(r, 0, sizeof r);
    r[0] = 0x01;
    r[2] = 0x01 | 0x08 | 0x10;              /* Select, Start, Up */
    r[3] = 0x40 | 0x04;                     /* Cross, L1 */
    r[4] = 0x01;                            /* PS */
    r[6] = 0x00; r[7] = 0xFF; r[8] = 0x80; r[9] = 0x7F;
    r[18] = 0x40; r[19] = 0xC0;             /* L2, R2 pressure */
    pad_state_reset(&st);
    CHECK(p->parse(ctx, r, sizeof r, &st) == PARSE_FULL);
    CHECK(st.buttons == (PAD_CREATE | PAD_OPTIONS | PAD_UP | PAD_CROSS | PAD_L1 | PAD_PS));
    CHECK(st.lx == 0x00 && st.ly == 0xFF && st.rx == 0x80 && st.ry == 0x7F);
    CHECK(st.l2 == 0x40 && st.r2 == 0xC0);
    r[0] = 0x02;
    CHECK(p->parse(ctx, r, sizeof r, &st) == PARSE_NONE);    /* not its input report */
    CHECK(p->parse(ctx, r, 10, &st) == PARSE_NONE);          /* too short */

    /* The wake-up: one step, the player LED (player 3: bit 3), then done. */
    n = p->init_report(ctx, 0, &o, out, sizeof out);
    CHECK(n == 36 && out[0] == 0x01 && out[10] == 0x08);
    CHECK(out[11] == 0xFF && out[12] == 0x27 && out[15] == 0x32);
    CHECK(p->init_report(ctx, 1, &o, out, sizeof out) == 0);
    o.strong = 200; o.weak = 10;
    n = p->build_output(ctx, &o, out, sizeof out);
    CHECK(n == 36 && out[3] == 1 && out[5] == 200);
}

int main(void)
{
    test_crc();
    test_lookup();
    test_ds4_short();
    test_ds5();
    test_xbox();
    test_switch();
    test_generic();
    test_ds3();
    printf(g_fail ? "%d check(s) failed\n" : "all profile checks passed\n", g_fail);
    return g_fail != 0;
}
