#ifndef AT_FRAME_H
#define AT_FRAME_H

#include <stdint.h>

#include "at_types.h"

/* Frame layout: Magic(4) + CRC8(1) + Size(2) + Opcode(2) + Status/Reserved(1)
 * = AT_FRAME_HEADER_SIZE bytes, followed by a variable-length payload.
 *
 * Byte order: ALL multi-byte fields are LITTLE-ENDIAN (matching the ARM
 * Cortex-M23 native layout, and the glasses-side protocol implementation).
 * Size holds the PAYLOAD length only (not the total frame length) — a frame
 * occupies Size + AT_FRAME_HEADER_SIZE bytes on the wire. On the wire the
 * REQ magic reads 23 54 41 23 ("#TA#") and the RSP magic 23 50 41 23 ("#PA#")
 * when bytes are listed in transmission order. */
#define AT_FRAME_HEADER_SIZE 10U
#define AT_FRAME_MAX_PAYLOAD 255U

#define AT_FRAME_MAGIC_REQ   0x23415423U /* LE on wire: 23 54 41 23 */
#define AT_FRAME_MAGIC_RSP   0x23415023U /* LE on wire: 23 50 41 23 */

/* Pack a request frame, writing the request magic and the reserved byte into
 * the Status/Reserved slot. payload may be NULL when payload_len is 0. Returns
 * the total frame length written to buf. */
uint16_t
at_frame_pack_request(uint8_t *buf, uint16_t opcode, const uint8_t *payload, uint8_t payload_len, uint8_t reserved);

/* Pack a response frame, writing the response magic and status. Returns the
 * total frame length written to buf. */
uint16_t
at_frame_pack_response(uint8_t *buf, uint16_t opcode, uint8_t status, const uint8_t *payload, uint8_t payload_len);

/* Validate and decode a received frame: checks the magic, the Size field
 * against total_len, and the CRC8 over the whole frame minus the CRC byte. On
 * success writes opcode, status and the payload and returns AT_SUCCESS;
 * otherwise returns the matching at_status code. A NULL output pointer is
 * skipped rather than dereferenced. */
at_status at_frame_parse(
    const uint8_t *buf, uint16_t total_len, uint16_t *opcode, uint8_t *status, uint8_t *payload, uint8_t *payload_len);

/* Frame-aware receive: hunt for the magic lead byte, read the 10-byte header,
 * parse the Size field, then read exactly `size` bytes. Returns the frame
 * length written to buf, or 0 on timeout. Each byte received resets the
 * timeout, so a steady stream never trips it; only a `timeout_ms` gap with no
 * new bytes fails. This is the rx side to pair with hal_usart_send — it
 * replaces hal_usart_send_recv for protocols with a known frame size, avoiding
 * the latter's full byte-gap timeout wait after the last byte lands.
 *
 * If expected_opcode is non-zero, the function peeks the opcode field from
 * the header BEFORE consuming any payload bytes. When the opcode does not
 * match, it returns 0 and leaves the frame in the ring buffer — so a caller
 * like charge_poll that expects a heartbeat response (0x3001) will not eat a
 * HIL command frame (0x3010+) that happens to arrive during its recv window.
 * Pass 0 to accept any opcode (legacy behaviour). */
uint16_t at_frame_recv(uint8_t *buf, uint16_t buf_max, uint32_t timeout_ms, uint16_t expected_opcode);

#endif /* AT_FRAME_H */
