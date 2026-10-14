/* Controller profiles: which pads are known by vendor and product id, how
 * their input reports read, and how to talk back to them.
 *
 * Adding a controller means adding one profile: an id table, a parser and,
 * if it has rumble, lights or needs waking up, the output builders.
 * Controllers with no profile go to the generic one (generic.c), which
 * reads the pad's own HID descriptor.
 */
#ifndef PH_PROFILES_H
#define PH_PROFILES_H

#include "pad.h"

#include <stddef.h>

/* Per-pad working memory a profile may use (packet counters, the generic
 * profile's report layout). */
#define PROFILE_CTX_MAX 2048

/* parse() results. */
#define PARSE_NONE  0       /* not a report this profile reads */
#define PARSE_OK    1
#define PARSE_FULL  2       /* the full report the init sequence asks for */
#define PARSE_REINIT 3      /* the pad dropped its report mode: send the wake-up sequence again */

typedef struct pad_profile {
    const char *name;
    const uint16_t (*ids)[2];       /* {vid, pid} pairs */
    int nids;

    /* Reads one input report (report id first, HID header stripped). */
    int (*parse)(void *ctx, const uint8_t *rep, int len, pad_state *st);

    /* Writes the output report for *out (report id first, HID header not
     * included) and returns its length, or 0 if there is none. */
    int (*build_output)(void *ctx, const pad_output *out, uint8_t *buf, int max);

    /* Writes step `step` of the pad's wake-up sequence, or returns 0 once
     * there are no more steps. NULL: the pad needs none. */
    int (*init_report)(void *ctx, int step, const pad_output *out, uint8_t *buf, int max);

    /* Repeat the wake-up sequence until parse() returns PARSE_FULL. */
    int needs_full;

    /* Resend a non-zero rumble this often (ms), for pads whose rumble
     * stops by itself. 0: never. */
    int rumble_refresh_ms;

    /* Prepares ctx for the exact model, when the profile covers several
     * that differ. NULL: nothing to prepare. */
    void (*setup)(void *ctx, uint16_t vid, uint16_t pid);
    /* Feature report asked for (HID GET_REPORT on the control channel) in
     * the repeat rounds, for pads that only switch to their full report
     * then, as some DualShock 4 clones do. 0: none. */
    uint8_t wake_feature;
} pad_profile;

const pad_profile *profile_find(uint16_t vid, uint16_t pid);

/* Major class 5 (peripheral) with a gamepad or joystick minor class. */
int profile_class_is_gamepad(uint32_t class_of_device);

/* ---- generic HID (generic.c) ---- */

extern const pad_profile generic_profile;

/* Prepares ctx for a pad with no profile from its HID report descriptor,
 * and the mapping file for vid:pid in map_dir if there is one (else the
 * default mapping). Returns 0 if the descriptor shows no usable gamepad. */
int generic_setup(void *ctx, const uint8_t *desc, int len,
                  uint16_t vid, uint16_t pid, const char *map_dir);

#endif
