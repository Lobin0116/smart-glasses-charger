#include <string.h>

#include "at_crc.h"
#include "at_frame.h"
#include "hal_timer.h"
#include "hal_usart.h"

#ifndef BL_NO_WWDGT
#include "hal_wwdgt.h"
#endif

#define AT_FRAME_OFFSET_CRC 4U
#define AT_FRAME_OFFSET_SIZE 5U
#define AT_FRAME_OFFSET_OPCODE 7U
#define AT_FRAME_OFFSET_STATUS 9U
#define AT_FRAME_OFFSET_PAYLOAD 10U

#define AT_FRAME_CRC_INIT 0x00U
volatile uint8_t at_frame_last_fail_stage = 0U;
volatile uint16_t at_frame_last_buf_bytes = 0U;

static void at_frame_put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t at_frame_get_le16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }

static void at_frame_put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t at_frame_get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t at_frame_crc(const uint8_t *buf, uint16_t total_len)
{
    return at_crc8((uint8_t *)buf + AT_FRAME_OFFSET_CRC + 1U,
                   (uint16_t)(total_len - (AT_FRAME_OFFSET_CRC + 1U)), AT_FRAME_CRC_INIT);
}

static uint16_t at_frame_pack(
    uint8_t *buf, uint32_t magic, uint16_t opcode, uint8_t status, const uint8_t *payload, uint8_t payload_len)
{
    uint16_t total_len = (uint16_t)(AT_FRAME_HEADER_SIZE + payload_len);

    at_frame_put_le32(buf, magic);

    at_frame_put_le16(buf + AT_FRAME_OFFSET_SIZE, payload_len);
    at_frame_put_le16(buf + AT_FRAME_OFFSET_OPCODE, opcode);
    buf[AT_FRAME_OFFSET_STATUS] = status;
    if (payload_len > 0U) {
        memcpy(buf + AT_FRAME_OFFSET_PAYLOAD, payload, payload_len);
    }
    buf[AT_FRAME_OFFSET_CRC] = at_frame_crc(buf, total_len);

    return total_len;
}

uint16_t
at_frame_pack_request(uint8_t *buf, uint16_t opcode, const uint8_t *payload, uint8_t payload_len, uint8_t reserved)
{
    return at_frame_pack(buf, AT_FRAME_MAGIC_REQ, opcode, reserved, payload, payload_len);
}

uint16_t
at_frame_pack_response(uint8_t *buf, uint16_t opcode, uint8_t status, const uint8_t *payload, uint8_t payload_len)
{
    return at_frame_pack(buf, AT_FRAME_MAGIC_RSP, opcode, status, payload, payload_len);
}

at_status at_frame_parse(
    const uint8_t *buf, uint16_t total_len, uint16_t *opcode, uint8_t *status, uint8_t *payload, uint8_t *payload_len)
{
    if (buf == NULL) {
        return AT_ERR_NULL;
    }
    if (total_len < AT_FRAME_HEADER_SIZE || total_len > (uint16_t)(AT_FRAME_HEADER_SIZE + AT_FRAME_MAX_PAYLOAD)) {
        return AT_ERR_LENGTH;
    }

    uint32_t magic = at_frame_get_le32(buf);
    if (magic != AT_FRAME_MAGIC_REQ && magic != AT_FRAME_MAGIC_RSP) {
        return AT_ERR_MAGIC;
    }

    if ((uint16_t)(at_frame_get_le16(buf + AT_FRAME_OFFSET_SIZE) + AT_FRAME_HEADER_SIZE) != total_len) {
        return AT_ERR_LENGTH;
    }

    if (buf[AT_FRAME_OFFSET_CRC] != at_frame_crc(buf, total_len)) {
        return AT_ERR_CRC;
    }

    uint8_t plen = (uint8_t)(total_len - AT_FRAME_HEADER_SIZE);

    if (opcode != NULL) {
        *opcode = at_frame_get_le16(buf + AT_FRAME_OFFSET_OPCODE);
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

uint16_t at_frame_recv(uint8_t *buf, uint16_t buf_max, uint32_t timeout_ms, uint16_t expected_opcode)
{
    uint32_t start = hal_timer_get_ms();
    uint8_t c;

    while (true) {
#ifndef BL_NO_WWDGT
        hal_wwdgt_feed();

#endif
        if (!hal_usart_rx_peek(&c)) {
            if (hal_timer_expired(start, timeout_ms)) {
                at_frame_last_fail_stage = 1U;
                at_frame_last_buf_bytes = hal_usart_rx_avail();
                return 0U;
            }
            continue;
        }
        if (c == 0x23U) {
            break;
        }
        (void)hal_usart_rx_get(&c);
    }

    uint8_t header[AT_FRAME_HEADER_SIZE];
    while (true) {
#ifndef BL_NO_WWDGT
        hal_wwdgt_feed();
#endif
        if (hal_usart_rx_peek_n(header, AT_FRAME_HEADER_SIZE)) {
            break;
        }
        if (hal_timer_expired(start, timeout_ms)) {
            at_frame_last_fail_stage = 2U;
            at_frame_last_buf_bytes = hal_usart_rx_avail();

            hal_usart_rx_clear();
            return 0U;
        }
    }

    uint32_t magic = at_frame_get_le32(header);
    if (magic != AT_FRAME_MAGIC_REQ && magic != AT_FRAME_MAGIC_RSP) {

        (void)hal_usart_rx_get(&c);
        at_frame_last_fail_stage = 3U;
        at_frame_last_buf_bytes = hal_usart_rx_avail();
        hal_usart_rx_clear();
        return 0U;
    }

    uint16_t size = at_frame_get_le16(header + AT_FRAME_OFFSET_SIZE);
    uint16_t opcode = at_frame_get_le16(header + AT_FRAME_OFFSET_OPCODE);
    uint16_t total_len = (uint16_t)(AT_FRAME_HEADER_SIZE + size);
    if (size > AT_FRAME_MAX_PAYLOAD || total_len > buf_max) {
        (void)hal_usart_rx_get(&c);
        at_frame_last_fail_stage = 4U;
        at_frame_last_buf_bytes = hal_usart_rx_avail();
        hal_usart_rx_clear();
        return 0U;
    }

    if (expected_opcode != 0U && opcode != expected_opcode) {

        return 0U;
    }

    for (uint16_t i = 0U; i < AT_FRAME_HEADER_SIZE; i++) {
        (void)hal_usart_rx_get(&buf[i]);
    }
    uint16_t n = AT_FRAME_HEADER_SIZE;
    while (n < total_len) {
#ifndef BL_NO_WWDGT
        hal_wwdgt_feed();
#endif
        if (hal_usart_rx_get(&buf[n])) {
            n++;
            start = hal_timer_get_ms();
        } else if (hal_timer_expired(start, timeout_ms)) {
            at_frame_last_fail_stage = 6U;
            at_frame_last_buf_bytes = hal_usart_rx_avail();

            hal_usart_rx_clear();
            return 0U;
        }
    }

    at_frame_last_fail_stage = 0U;
    at_frame_last_buf_bytes = 0U;

    hal_usart_rx_clear();
    return n;
}
