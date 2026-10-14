/* /data/anypad/config.ini: the few things worth changing.
 *
 *   menu_combo   = options+l2+r2      buttons held to open the menu on the console
 *   menu_hold_ms = 1500                how long they are held (500..5000)
 *
 * Button names: l1 r1 l2 r2 l3 r3 cross circle square triangle up down left
 * right options create touchpad. At least two. The PS button cannot be read.
 * Lines starting with # are comments. A line that is not understood is
 * reported and skipped; the defaults stand for anything missing or wrong. */
#ifndef ANYPAD_CONFIG_H
#define ANYPAD_CONFIG_H

#include "pad.h"

#include <stddef.h>

typedef struct {
    uint32_t menu_combo;
    int menu_hold_ms;
} anypad_config;

void config_defaults(anypad_config *c);

/* Reads `path`. Returns the number of lines not understood; a missing file
 * is not an error (returns 0, defaults kept). */
int config_load(anypad_config *c, const char *path);

/* "options+l2+r2" -> mask. Returns 0 if a name is unknown or fewer than two
 * buttons are named. */
int config_parse_combo(const char *text, uint32_t *mask);

/* Mask -> words for a person: "L2 + R2 + OPTIONS". */
void config_combo_text(uint32_t mask, char *out, size_t max);

/* Like config_parse_combo, but one button is enough. */
int config_parse_keys(const char *text, uint32_t *mask);

/* Mask -> the names config_parse_combo reads back: "l2+r2+options". */
void config_combo_keys(uint32_t mask, char *out, size_t max);

/* «Configure PS»: a button combination chosen by the user that stands for
 * the PS button, per controller model (vid:pid), for controllers whose own
 * PS / Home button is missing or not read. Kept in /data/anypad/ps_combos.txt,
 * one line per model: "057e:2009 l1+l2+options". One to three buttons. */
#define PSCOMBO_MODELS 16
#define PSCOMBO_MAX_BUTTONS 3
int      pscombo_load(const char *path);          /* returns the models read */
uint32_t pscombo_get(uint16_t vid, uint16_t pid);  /* 0: none */
/* Sets (mask != 0) or removes (mask == 0) the model's combination and
 * rewrites the file. Returns 0 for more than PSCOMBO_MAX_BUTTONS buttons,
 * PS itself, or no room. */
int      pscombo_set(const char *path, uint16_t vid, uint16_t pid, uint32_t mask);
/* The buttons the console gets: while the whole combination is held, PS
 * instead of its buttons; anything else, unchanged. */
uint32_t pscombo_apply(uint32_t combo, uint32_t buttons);

#endif
