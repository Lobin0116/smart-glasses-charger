#ifndef AT_FRAME_H
#define AT_FRAME_H

#include <stdint.h>

#include "at_types.h"

/* Frame layout: Magic(4) + CRC8(1) + Size(2) + Opcode(2) + Status/Reserved(1)
 * = AT_FRAME_HEADER_SIZE bytes, followed by a variable-length payload. Size
 * holds the total frame length (header + payload). Multi-byte fields are
 * big-endian so the magic reads as the ASCII tag "#AT#" / "#AP#" on the wire. */
#define AT_FRAME_HEADER_SIZE 10U
#define AT_FRAME_MAX_PAYLOAD 255U

#define AT_FRAME_MAGIC_REQ 0x23415423U /* "#AT#" */
#define AT_FRAME_MAGIC_RSP 0x23415023U /* "#AP#" */

/* Pack a request frame, writing the request magic and the reserved byte into
 * the Status/Reserved slot. payload may be NULL when payload_len is 0. Returns
 * the total frame length written to buf. */
uint16_t at_frame_pack_request(uint8_t *buf, uint16_t opcode, const uint8_t *payload,
                               uint8_t payload_len, uint8_t reserved);

/* Pack a response frame, writing the response magic and status. Returns the
 * total frame length written to buf. */
uint16_t at_frame_pack_response(uint8_t *buf, uint16_t opcode, uint8_t status,
                                const uint8_t *payload, uint8_t payload_len);

/* Validate and decode a received frame: checks the magic, the Size field
 * against total_len, and the CRC8 over the whole frame minus the CRC byte. On
 * success writes opcode, status and the payload and returns AT_SUCCESS;
 * otherwise returns the matching at_status code. A NULL output pointer is
 * skipped rather than dereferenced. */
at_status at_frame_parse(const uint8_t *buf, uint16_t total_len, uint16_t *opcode, uint8_t *status,
                         uint8_t *payload, uint8_t *payload_len);

#endif /* AT_FRAME_H */
