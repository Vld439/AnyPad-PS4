#include "config.h"
#include "log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *key; const char *shown; uint32_t bit; } k_buttons[] = {
    { "l1", "L1", PAD_L1 }, { "r1", "R1", PAD_R1 }, { "l2", "L2", PAD_L2 }, { "r2", "R2", PAD_R2 },
    { "l3", "L3", PAD_L3 }, { "r3", "R3", PAD_R3 },
    { "cross", "cruz", PAD_CROSS }, { "circle", "círculo", PAD_CIRCLE },
    { "square", "cuadrado", PAD_SQUARE }, { "triangle", "triángulo", PAD_TRIANGLE },
    { "up", "arriba", PAD_UP }, { "down", "abajo", PAD_DOWN },
    { "left", "izquierda", PAD_LEFT }, { "right", "derecha", PAD_RIGHT },
    { "options", "OPTIONS", PAD_OPTIONS }, { "create", "CREATE", PAD_CREATE },
    { "touchpad", "panel táctil", PAD_TOUCHPAD },
};
#define N_BUTTONS (sizeof k_buttons / sizeof k_buttons[0])

void config_defaults(anypad_config *c)
{
    c->menu_combo = PAD_OPTIONS | PAD_L2 | PAD_R2;
    c->menu_hold_ms = 1500;
}

static int parse_buttons(const char *text, uint32_t *mask, int least);

int config_parse_combo(const char *text, uint32_t *mask)
{
    return parse_buttons(text, mask, 2);
}

int config_parse_keys(const char *text, uint32_t *mask)
{
    return parse_buttons(text, mask, 1);
}

static int parse_buttons(const char *text, uint32_t *mask, int least)
{
    char buf[96], *tok, *save = NULL;
    uint32_t m = 0;
    int n = 0;

    if (!text || strlen(text) >= sizeof buf) return 0;
    strcpy(buf, text);
    for (tok = strtok_r(buf, "+ ", &save); tok; tok = strtok_r(NULL, "+ ", &save)) {
        size_t i;
        char *p;
        for (p = tok; *p; p++) *p = (char)tolower((unsigned char)*p);
        for (i = 0; i < N_BUTTONS; i++)
            if (strcmp(tok, k_buttons[i].key) == 0) break;
        if (i == N_BUTTONS) return 0;               /* unknown name */
        if (!(m & k_buttons[i].bit)) n++;
        m |= k_buttons[i].bit;
    }
    if (n < least) return 0;
    *mask = m;
    return 1;
}

void config_combo_text(uint32_t mask, char *out, size_t max)
{
    size_t i, n = 0;

    out[0] = '\0';
    for (i = 0; i < N_BUTTONS; i++) {
        int w;
        if (!(mask & k_buttons[i].bit)) continue;
        w = snprintf(out + n, max - n, "%s%s", n ? " + " : "", k_buttons[i].shown);
        if (w < 0 || (size_t)w >= max - n) break;
        n += (size_t)w;
    }
}

static char *trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = '\0';
    return s;
}

int config_load(anypad_config *c, const char *path)
{
    char line[160];
    FILE *f = fopen(path, "r");
    int bad = 0, no = 0;

    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *eq, *key, *val;
        no++;
        key = trim(line);
        if (!*key || *key == '#') continue;
        eq = strchr(key, '=');
        if (!eq) {
            log_line("config %s:%d: not understood", path, no);
            bad++;
            continue;
        }
        *eq = '\0';
        val = trim(eq + 1);
        key = trim(key);
        if (strcmp(key, "menu_combo") == 0) {
            uint32_t m;
            if (config_parse_combo(val, &m)) {
                c->menu_combo = m;
                continue;
            }
        } else if (strcmp(key, "menu_hold_ms") == 0) {
            int v = atoi(val);
            if (v >= 500 && v <= 5000) {
                c->menu_hold_ms = v;
                continue;
            }
        }
        log_line("config %s:%d: '%s' is not valid, default kept", path, no, key);
        bad++;
    }
    fclose(f);
    return bad;
}

void config_combo_keys(uint32_t mask, char *out, size_t max)
{
    size_t i, n = 0;

    out[0] = '\0';
    for (i = 0; i < N_BUTTONS; i++) {
        int w;
        if (!(mask & k_buttons[i].bit)) continue;
        w = snprintf(out + n, max - n, "%s%s", n ? "+" : "", k_buttons[i].key);
        if (w < 0 || (size_t)w >= max - n) break;
        n += (size_t)w;
    }
}

/* ---- «Configure PS» -------------------------------------------------------- */

static struct { uint16_t vid, pid; uint32_t mask; } g_pscombo[PSCOMBO_MODELS];
static int g_npscombo;

static int count_bits(uint32_t m)
{
    int n = 0;

    for (; m; m &= m - 1) n++;
    return n;
}

int pscombo_load(const char *path)
{
    char line[160];
    FILE *f = fopen(path, "r");

    g_npscombo = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f) && g_npscombo < PSCOMBO_MODELS) {
        unsigned vid, pid;
        char combo[100];
        uint32_t mask;
        if (sscanf(line, "%4x:%4x %99s", &vid, &pid, combo) != 3) continue;
        if (!config_parse_keys(combo, &mask) || count_bits(mask) > PSCOMBO_MAX_BUTTONS) continue;
        g_pscombo[g_npscombo].vid = (uint16_t)vid;
        g_pscombo[g_npscombo].pid = (uint16_t)pid;
        g_pscombo[g_npscombo].mask = mask;
        g_npscombo++;
    }
    fclose(f);
    return g_npscombo;
}

uint32_t pscombo_get(uint16_t vid, uint16_t pid)
{
    int i;

    for (i = 0; i < g_npscombo; i++)
        if (g_pscombo[i].vid == vid && g_pscombo[i].pid == pid) return g_pscombo[i].mask;
    return 0;
}

int pscombo_set(const char *path, uint16_t vid, uint16_t pid, uint32_t mask)
{
    FILE *f;
    int i;

    if (mask && (count_bits(mask) > PSCOMBO_MAX_BUTTONS || (mask & PAD_PS))) return 0;
    for (i = 0; i < g_npscombo; i++)
        if (g_pscombo[i].vid == vid && g_pscombo[i].pid == pid) break;
    if (i == g_npscombo) {
        if (!mask) return 1;                    /* nothing to remove */
        if (g_npscombo == PSCOMBO_MODELS) return 0;
        g_npscombo++;
    }
    if (mask) {
        g_pscombo[i].vid = vid;
        g_pscombo[i].pid = pid;
        g_pscombo[i].mask = mask;
    } else {
        g_pscombo[i] = g_pscombo[--g_npscombo];
    }
    if (!path) return 1;
    f = fopen(path, "w");
    if (!f) return 0;
    for (i = 0; i < g_npscombo; i++) {
        char keys[100];
        config_combo_keys(g_pscombo[i].mask, keys, sizeof keys);
        fprintf(f, "%04x:%04x %s\n", g_pscombo[i].vid, g_pscombo[i].pid, keys);
    }
    fclose(f);
    return 1;
}

uint32_t pscombo_apply(uint32_t combo, uint32_t buttons)
{
    if (!combo || (buttons & combo) != combo) return buttons;
    return (buttons & ~combo) | PAD_PS;
}
