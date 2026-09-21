#ifndef AT_FRAME_H
#define AT_FRAME_H

#include <stdint.h>

#include "at_types.h"
#define AT_FRAME_HEADER_SIZE 10U
#define AT_FRAME_MAX_PAYLOAD 255U

#define AT_FRAME_MAGIC_REQ 0x23415423U
#define AT_FRAME_MAGIC_RSP 0x23415023U

uint16_t
at_frame_pack_request(uint8_t *buf, uint16_t opcode, const uint8_t *payload, uint8_t payload_len, uint8_t reserved);

uint16_t
at_frame_pack_response(uint8_t *buf, uint16_t opcode, uint8_t status, const uint8_t *payload, uint8_t payload_len);

at_status at_frame_parse(
    const uint8_t *buf, uint16_t total_len, uint16_t *opcode, uint8_t *status, uint8_t *payload, uint8_t *payload_len);
uint16_t at_frame_recv(uint8_t *buf, uint16_t buf_max, uint32_t timeout_ms, uint16_t expected_opcode);

#endif
