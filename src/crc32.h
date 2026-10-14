/* CRC-32 (IEEE, reflected) as Sony's controllers use it over Bluetooth. */
#ifndef PH_CRC32_H
#define PH_CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t crc32_calc(const uint8_t *data, size_t len);

/* Appends the CRC a Bluetooth output report carries: computed over the HID
 * header byte 0xA2 followed by the report (id first), stored little-endian
 * in the report's last four bytes. `len` counts those four bytes. */
void crc32_seal_output(uint8_t *report, int len);

#endif
