/* config, hotkey and netinfo. */
#include "../src/config.h"
#include "../src/hotkey.h"
#include "../src/netinfo.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void test_config(void)
{
    anypad_config c;
    uint32_t m;
    char text[96];
    FILE *f;

    printf("config: defaults, parsing, bad lines\n");
    config_defaults(&c);
    CHECK(c.menu_combo == (PAD_OPTIONS | PAD_L2 | PAD_R2) && c.menu_hold_ms == 1500);
    config_combo_text(c.menu_combo, text, sizeof text);
    CHECK(strcmp(text, "L2 + R2 + OPTIONS") == 0);

    CHECK(config_parse_combo("L1+R1", &m) && m == (PAD_L1 | PAD_R1));
    CHECK(config_parse_combo("options + create", &m) && m == (PAD_OPTIONS | PAD_CREATE));
    CHECK(!config_parse_combo("l1", &m));               /* one button: refused */
    CHECK(!config_parse_combo("l1+l1", &m));
    CHECK(!config_parse_combo("l1+ps", &m));            /* the PS button cannot be read */
    CHECK(!config_parse_combo("l1+banana", &m));
    CHECK(!config_parse_combo("", &m));

    CHECK(config_load(&c, "build/no_such_config.ini") == 0);        /* missing: fine */
    f = fopen("build/test_config.ini", "w");
    fputs("# comment\nmenu_combo = l2+r2+triangle\nmenu_hold_ms = 2000\n"
          "menu_hold_ms = 99\nmenu_combo = l1\nnonsense\nunknown_key = 1\n", f);
    fclose(f);
    CHECK(config_load(&c, "build/test_config.ini") == 4);
    CHECK(c.menu_combo == (PAD_L2 | PAD_R2 | PAD_TRIANGLE));        /* the bad later ones did not win */
    CHECK(c.menu_hold_ms == 2000);
}

static void test_hotkey(void)
{
    const uint32_t combo = PAD_OPTIONS | PAD_L2 | PAD_R2;
    hotkey h;
    long t = 10000;

    printf("hotkey: held long enough, once, with letting go and a cooldown\n");
    hotkey_init(&h, combo, 1500);
    CHECK(!hotkey_update(&h, 1, combo, t));
    CHECK(!hotkey_update(&h, 1, combo, t + 1400));
    CHECK(hotkey_update(&h, 1, combo, t + 1500));                   /* fires */
    CHECK(!hotkey_update(&h, 1, combo, t + 3000));                  /* still held: not again */
    CHECK(!hotkey_update(&h, 1, 0, t + 3100));                      /* let go */
    CHECK(!hotkey_update(&h, 1, combo, t + 3200));                  /* held again, cooldown */
    CHECK(!hotkey_update(&h, 1, combo, t + 4800));
    CHECK(hotkey_update(&h, 1, combo, t + 6600));                   /* held 3.4 s, cooldown over */

    hotkey_init(&h, combo, 1500);
    t = 50000;
    hotkey_update(&h, 1, combo, t);
    CHECK(!hotkey_update(&h, 1, PAD_L2 | PAD_R2, t + 1000));        /* one let go: restart */
    CHECK(!hotkey_update(&h, 1, combo, t + 1100));
    CHECK(!hotkey_update(&h, 1, combo, t + 2500));                  /* only 1.4 s since the restart */
    CHECK(hotkey_update(&h, 1, combo, t + 2600));

    hotkey_init(&h, combo, 1500);
    t = 90000;
    hotkey_update(&h, 1, combo, t);
    CHECK(!hotkey_update(&h, 0, 0, t + 1000));                      /* the pad stopped answering */
    CHECK(!hotkey_update(&h, 1, combo, t + 2000));                  /* back, but not armed until let go */
    CHECK(!hotkey_update(&h, 1, combo, t + 4000));
    hotkey_update(&h, 1, 0, t + 4100);
    hotkey_update(&h, 1, combo, t + 4200);
    CHECK(hotkey_update(&h, 1, combo, t + 5800));
}

static void test_netinfo(void)
{
    char ip[16];
    int ok = local_ip(ip);

    printf("netinfo: an address, or loopback without a route\n");
    CHECK(strlen(ip) >= 7 && strlen(ip) <= 15);
    if (!ok) CHECK(strcmp(ip, "127.0.0.1") == 0);
}

static void test_pscombo(void)
{
    const char *path = "build/test_ps_combos.txt";
    uint32_t m = 0;
    char keys[100];

    printf("«Configure PS»: a combination per model stands for PS, and is kept\n");
    remove(path);
    CHECK(pscombo_load(path) == 0);
    CHECK(config_parse_keys("l1+l2+options", &m));
    CHECK(pscombo_set(path, 0x057e, 0x2009, m));
    CHECK(pscombo_get(0x057e, 0x2009) == m);
    CHECK(pscombo_get(0x054c, 0x09cc) == 0);
    /* held whole: PS instead of its buttons; the rest goes through */
    CHECK(pscombo_apply(m, PAD_L1 | PAD_L2 | PAD_OPTIONS | PAD_CROSS) == (PAD_PS | PAD_CROSS));
    CHECK(pscombo_apply(m, PAD_L1 | PAD_L2) == (PAD_L1 | PAD_L2));      /* not whole: unchanged */
    CHECK(pscombo_apply(0, PAD_L1) == PAD_L1);
    /* kept across runs */
    CHECK(pscombo_load(path) == 1);
    CHECK(pscombo_get(0x057e, 0x2009) == m);
    config_combo_keys(m, keys, sizeof keys);
    CHECK(strcmp(keys, "l1+l2+options") == 0);
    /* one to three buttons, never PS itself */
    CHECK(pscombo_set(path, 1, 1, PAD_TOUCHPAD));                 /* one button alone */
    CHECK(pscombo_apply(PAD_TOUCHPAD, PAD_TOUCHPAD | PAD_CROSS) == (PAD_PS | PAD_CROSS));
    CHECK(pscombo_set(path, 1, 1, 0));
    CHECK(!pscombo_set(path, 1, 1, PAD_L1 | PAD_R1 | PAD_L2 | PAD_R2));
    CHECK(!pscombo_set(path, 1, 1, PAD_L1 | PAD_PS));
    CHECK(config_parse_keys("touchpad", &m) && m == PAD_TOUCHPAD);
    CHECK(!config_parse_combo("touchpad", &m));                    /* the menu combo still needs two */
    /* removed */
    CHECK(pscombo_set(path, 0x057e, 0x2009, 0));
    CHECK(pscombo_get(0x057e, 0x2009) == 0);
    CHECK(pscombo_load(path) == 0);
    remove(path);
}

int main(void)
{
    test_config();
    test_pscombo();
    test_hotkey();
    test_netinfo();
    printf(g_fail ? "%d check(s) failed\n" : "all config checks passed\n", g_fail);
    return g_fail != 0;
}
