#include "usb_desc.h"

#include <string.h>

#define DT_INTERFACE 0x04
#define DT_ENDPOINT  0x05

int usb_find_hci_functions(const uint8_t *cfg, int len, hci_function *out)
{
    int off = 0, n = 0, cur = -1;      /* cur: index in out of the interface being read */
    int in_class = 0;

    while (off + 2 <= len) {
        int dl = cfg[off];
        const uint8_t *d = cfg + off;

        if (dl < 2 || off + dl > len) break;            /* malformed: stop */

        if (d[1] == DT_INTERFACE && dl >= 9) {
            /* Only alternate setting 0 of a Bluetooth-class interface. */
            in_class = d[5] == 0xE0 && d[6] == 0x01 && d[7] == 0x01 && d[3] == 0;
            cur = -1;
            if (in_class && n < HCI_FUNCTIONS_MAX) {
                cur = n++;
                memset(&out[cur], 0, sizeof out[cur]);
                out[cur].iface = d[2];
            }
        } else if (d[1] == DT_ENDPOINT && dl >= 7 && in_class && cur >= 0) {
            uint8_t addr = d[2], attr = d[3] & 3;
            uint16_t mps = (uint16_t)(d[4] | d[5] << 8);
            hci_function *f = &out[cur];

            if (attr == 3 && (addr & 0x80) && !f->ep_events) {
                f->ep_events = addr;
                f->mps_events = mps;
            } else if (attr == 2 && (addr & 0x80) && !f->ep_acl_in) {
                f->ep_acl_in = addr;
                f->mps_acl_in = mps;
            } else if (attr == 2 && !(addr & 0x80) && !f->ep_acl_out) {
                f->ep_acl_out = addr;
                f->mps_acl_out = mps;
            } else if (f->n_other < (int)sizeof f->other) {
                f->other[f->n_other++] = addr;
            }
        }
        off += dl;
    }

    /* Keep only the interfaces that really are an HCI function: the voice
     * interface of the same class has isochronous endpoints and none of these. */
    {
        int i, k = 0;
        for (i = 0; i < n; i++) {
            if (out[i].ep_events && out[i].ep_acl_in && out[i].ep_acl_out) {
                if (k != i) out[k] = out[i];
                k++;
            }
        }
        return k;
    }
}

uint8_t usb_spare_acl_out(const hci_function *f)
{
    int i;

    if (!f || (f->ep_acl_out & 0x0F) == (f->ep_acl_in & 0x0F)) return 0;
    for (i = 0; i < f->n_other && i < (int)sizeof f->other; i++)
        if (!(f->other[i] & 0x80) && (f->other[i] & 0x0F) == (f->ep_acl_in & 0x0F)) return f->other[i];
    return 0;
}
