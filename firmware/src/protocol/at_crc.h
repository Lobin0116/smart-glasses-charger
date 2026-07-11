#ifndef AT_CRC_H
#define AT_CRC_H

#include <stdint.h>

/* Continue a CRC-8 over len bytes at ptr, seeded with crc_origin. The protocol
 * uses a 256-entry lookup table with an initial value of 0x00. */
uint8_t at_crc8(uint8_t *ptr, uint16_t len, uint8_t crc_origin);

#endif /* AT_CRC_H */
