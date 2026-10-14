/* Malformed input: random and mutated bytes through every parser a pad
 * can reach. Run under the sanitizers (make fuzz) it must find nothing. */
#include "../src/profiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sdp_find_ids(const unsigned char *p, int n, uint16_t *vid, uint16_t *pid);

static unsigned g_seed = 12345;
static unsigned rnd(void) { g_seed = g_seed * 1103515245u + 12345u; return g_seed >> 8; }

/* A valid gamepad descriptor, to mutate as well as plain noise. */
static const unsigned char k_desc[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x04,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x07, 0x75, 0x04, 0x95, 0x01, 0x09, 0x39, 0x81, 0x42,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x10, 0x81, 0x02, 0xC0,
};

int main(void)
{
    static const uint16_t ids[][2] = {
        { 0x054C, 0x05C4 }, { 0x054C, 0x0CE6 }, { 0x045E, 0x0B13 }, { 0x057E, 0x2009 },
    };
    unsigned char buf[600], ctx[PROFILE_CTX_MAX];
    long i;
    int p, accepted = 0;

    for (i = 0; i < 200000; i++) {
        int n = (int)(rnd() % sizeof buf) + 1, k;
        pad_state st;
        uint16_t vid, pid;

        if (i & 1) {
            for (k = 0; k < n; k++) buf[k] = (unsigned char)rnd();
        } else {
            n = (int)sizeof k_desc;
            memcpy(buf, k_desc, sizeof k_desc);
            for (k = 0; k < 3; k++) buf[rnd() % sizeof k_desc] = (unsigned char)rnd();
        }

        if (generic_setup(ctx, buf, n, 1, 2, NULL)) {
            accepted++;
            for (k = 0; k < 8; k++) {
                int m = (int)(rnd() % 64) + 1, j;
                unsigned char rep[64];
                for (j = 0; j < m; j++) rep[j] = (unsigned char)rnd();
                pad_state_reset(&st);
                generic_profile.parse(ctx, rep, m, &st);
            }
        }
        for (p = 0; p < 4; p++) {
            const pad_profile *prof = profile_find(ids[p][0], ids[p][1]);
            memset(ctx, 0, sizeof ctx);
            if (prof->setup) prof->setup(ctx, ids[p][0], ids[p][1]);
            pad_state_reset(&st);
            prof->parse(ctx, buf, n < 80 ? n : 80, &st);
        }
        sdp_find_ids(buf, n, &vid, &pid);
    }
    printf("fuzz: 200000 inputs, %d descriptors accepted, no fault\n", accepted);
    return 0;
}
