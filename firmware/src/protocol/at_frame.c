#include <string.h>

#include "at_crc.h"
#include "at_frame.h"

#define AT_FRAME_OFFSET_CRC 4U
#define AT_FRAME_OFFSET_SIZE 5U
#define AT_FRAME_OFFSET_OPCODE 7U
#define AT_FRAME_OFFSET_STATUS 9U
#define AT_FRAME_OFFSET_PAYLOAD 10U

#define AT_FRAME_CRC_INIT 0x00U

static void at_frame_put_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static uint16_t at_frame_get_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void at_frame_put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t at_frame_get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* CRC8 spans the whole frame except the CRC byte: the magic, then everything
 * past the CRC byte through the last payload byte. */
static uint8_t at_frame_crc(const uint8_t *buf, uint16_t total_len) {
    uint8_t crc = at_crc8((uint8_t *)buf, AT_FRAME_OFFSET_CRC, AT_FRAME_CRC_INIT);
    crc = at_crc8((uint8_t *)buf + AT_FRAME_OFFSET_CRC + 1U,
                  (uint16_t)(total_len - (AT_FRAME_OFFSET_CRC + 1U)), crc);
    return crc;
}

static uint16_t at_frame_pack(uint8_t *buf, uint32_t magic, uint16_t opcode, uint8_t status,
                              const uint8_t *payload, uint8_t payload_len) {
    uint16_t total_len = (uint16_t)(AT_FRAME_HEADER_SIZE + payload_len);

    at_frame_put_be32(buf, magic);
    at_frame_put_be16(buf + AT_FRAME_OFFSET_SIZE, total_len);
    at_frame_put_be16(buf + AT_FRAME_OFFSET_OPCODE, opcode);
    buf[AT_FRAME_OFFSET_STATUS] = status;
    if (payload_len > 0U) {
        memcpy(buf + AT_FRAME_OFFSET_PAYLOAD, payload, payload_len);
    }
    buf[AT_FRAME_OFFSET_CRC] = at_frame_crc(buf, total_len);

    return total_len;
}

uint16_t at_frame_pack_request(uint8_t *buf, uint16_t opcode, const uint8_t *payload,
                               uint8_t payload_len, uint8_t reserved) {
    return at_frame_pack(buf, AT_FRAME_MAGIC_REQ, opcode, reserved, payload, payload_len);
}

uint16_t at_frame_pack_response(uint8_t *buf, uint16_t opcode, uint8_t status,
                                const uint8_t *payload, uint8_t payload_len) {
    return at_frame_pack(buf, AT_FRAME_MAGIC_RSP, opcode, status, payload, payload_len);
}

at_status at_frame_parse(const uint8_t *buf, uint16_t total_len, uint16_t *opcode, uint8_t *status,
                         uint8_t *payload, uint8_t *payload_len) {
    if (buf == NULL) {
        return AT_ERR_NULL;
    }
    if (total_len < AT_FRAME_HEADER_SIZE ||
        total_len > (uint16_t)(AT_FRAME_HEADER_SIZE + AT_FRAME_MAX_PAYLOAD)) {
        return AT_ERR_LENGTH;
    }

    uint32_t magic = at_frame_get_be32(buf);
    if (magic != AT_FRAME_MAGIC_REQ && magic != AT_FRAME_MAGIC_RSP) {
        return AT_ERR_MAGIC;
    }

    if (at_frame_get_be16(buf + AT_FRAME_OFFSET_SIZE) != total_len) {
        return AT_ERR_LENGTH;
    }

    if (buf[AT_FRAME_OFFSET_CRC] != at_frame_crc(buf, total_len)) {
        return AT_ERR_CRC;
    }

    uint8_t plen = (uint8_t)(total_len - AT_FRAME_HEADER_SIZE);

    if (opcode != NULL) {
        *opcode = at_frame_get_be16(buf + AT_FRAME_OFFSET_OPCODE);
    }
    if (status != NULL) {
        *status = buf[AT_FRAME_OFFSET_STATUS];
    }
    if (payload != NULL && plen > 0U) {
        memcpy(payload, buf + AT_FRAME_OFFSET_PAYLOAD, plen);
    }
    if (payload_len != NULL) {
        *payload_len = plen;
    }

    return AT_SUCCESS;
}
