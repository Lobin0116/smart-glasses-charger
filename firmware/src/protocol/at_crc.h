#ifndef AT_CRC_H
#define AT_CRC_H

#include <stdint.h>

uint8_t at_crc8(uint8_t *ptr, uint16_t len, uint8_t crc_origin);

#endif
