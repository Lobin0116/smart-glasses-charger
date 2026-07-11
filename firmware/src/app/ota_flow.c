#include <stddef.h>
#include <string.h>

#include "ota_flow.h"

#include "at_frame.h"
#include "at_opcode.h"
#include "at_types.h"
#include "hal_fwdgt.h"
#include "hal_timer.h"
#include "hal_usart.h"

/* Timing budget from CONTEXT.md "通信超时": the AT request/response cycle is
 * 100 ms, so each exchange below uses that as its deadline. */
#define OTA_TIMEOUT_MS 100U

/* Heartbeats sent while waiting for the glasses to agree, spaced one cycle apart
 * so the case keeps holding the glass in-box during the wait. */
#define OTA_REQUEST_RETRIES 3U
#define OTA_REQUEST_GAP_MS 100U

/* Bounded retries for the prepare and per-block exchanges. */
#define OTA_EXCHANGE_RETRIES 3U

/* Data bytes pulled per read request. The response carries a 5-byte header
 * (role + index + type) ahead of the data, so 240 keeps the response payload
 * under AT_FRAME_MAX_PAYLOAD with margin. */
#define OTA_BLOCK_SIZE 240U

/* Hard ceiling on the block count: bounds the loop if the glasses never reports
 * the end marker. Generous enough for a full flash image at the block size above. */
#define OTA_MAX_BLOCKS 1024U

/* Return codes from ota_run(). */
#define OTA_OK 0
#define OTA_ERR_REQUEST (-1)
#define OTA_ERR_PREPARE (-2)
#define OTA_ERR_READ (-3)
#define OTA_ERR_RUNAWAY (-4)

/* Whole frames live in static buffers: ota_run() is synchronous and not
 * re-entrant, and keeping these off the stack matters on the 8 KB SRAM part. */
#define OTA_BUF_SIZE (AT_FRAME_HEADER_SIZE + AT_FRAME_MAX_PAYLOAD)
static uint8_t ota_tx_buf[OTA_BUF_SIZE];
static uint8_t ota_rx_buf[OTA_BUF_SIZE];
static uint8_t ota_payload[AT_FRAME_MAX_PAYLOAD];

/* Tracks whether a transfer is in progress; cleared by ota_init() and on exit. */
static bool ota_active;

/* One heartbeat exchange: send a heartbeat carrying the current case status with
 * the OTA flag set or cleared per request_ota, then refresh the glass status from
 * the reply. Returns true on a valid reply and reports the agree flag. */
static bool ota_heartbeat(sm_ctx_t *ctx, bool request_ota, bool *agreed) {
    at_case_data req;
    req.role.des = AT_CASE_ROLE_GLASS;
    req.role.src = AT_CASE_ROLE_CASE;
    req.case_soc = (uint8_t)((ctx->case_soc & 0x7FU) | (ctx->glass_charging ? 0x80U : 0x00U));
    req.case_sta = (uint8_t)((ctx->lid_open ? 0x01U : 0x00U) | (request_ota ? 0x80U : 0x00U));

    uint16_t tx_len = at_frame_pack_request(ota_tx_buf, AT_OPCODE_CASE_HEART, (const uint8_t *)&req,
                                            (uint8_t)sizeof(req), 0U);
    uint16_t rx_len =
        hal_usart_send_recv(ota_tx_buf, tx_len, ota_rx_buf, OTA_BUF_SIZE, OTA_TIMEOUT_MS);
    if (rx_len == 0U) {
        return false;
    }

    uint16_t opcode = 0U;
    uint8_t status = AT_ERR_UNKNOWN;
    uint8_t plen = 0U;
    if (at_frame_parse(ota_rx_buf, rx_len, &opcode, &status, ota_payload, &plen) != AT_SUCCESS) {
        return false;
    }
    if (opcode != AT_OPCODE_CASE_HEART || status != AT_SUCCESS) {
        return false;
    }
    if (plen < (uint8_t)sizeof(at_glass_data)) {
        return false;
    }

    const at_glass_data *rsp = (const at_glass_data *)ota_payload;
    ctx->glass_present = true;
    ctx->glass_soc = (uint8_t)(rsp->glass_soc & 0x7FU);
    ctx->glass_full = (rsp->glass_soc & 0x80U) != 0U;
    if (agreed != NULL) {
        *agreed = (rsp->glass_sta & 0x80U) != 0U;
    }
    return true;
}

void ota_init(void) { ota_active = false; }

bool ota_request(sm_ctx_t *ctx) {
    for (uint8_t i = 0U; i < OTA_REQUEST_RETRIES; i++) {
        hal_fwdgt_feed();
        if (i > 0U) {
            hal_timer_delay_ms(OTA_REQUEST_GAP_MS);
        }
        bool agreed = false;
        if (ota_heartbeat(ctx, true, &agreed) && agreed) {
            ota_active = true;
            return true;
        }
    }
    return false;
}

bool ota_prepare(uint32_t *fw_size) {
    at_case_role req;
    req.des = AT_CASE_ROLE_GLASS;
    req.src = AT_CASE_ROLE_CASE;

    for (uint8_t i = 0U; i < OTA_EXCHANGE_RETRIES; i++) {
        hal_fwdgt_feed();
        uint16_t tx_len = at_frame_pack_request(ota_tx_buf, AT_OPCODE_CASE_PACKET_PREPARE,
                                                (const uint8_t *)&req, (uint8_t)sizeof(req), 0U);
        uint16_t rx_len =
            hal_usart_send_recv(ota_tx_buf, tx_len, ota_rx_buf, OTA_BUF_SIZE, OTA_TIMEOUT_MS);
        if (rx_len == 0U) {
            continue;
        }

        uint16_t opcode = 0U;
        uint8_t status = AT_ERR_UNKNOWN;
        uint8_t plen = 0U;
        if (at_frame_parse(ota_rx_buf, rx_len, &opcode, &status, ota_payload, &plen) !=
            AT_SUCCESS) {
            continue;
        }
        if (opcode != AT_OPCODE_CASE_PACKET_PREPARE || status != AT_SUCCESS) {
            continue;
        }
        if (plen < (uint8_t)sizeof(at_case_packet_prepare)) {
            continue;
        }

        at_case_packet_prepare rsp;
        memcpy(&rsp, ota_payload, sizeof(rsp));
        if (fw_size != NULL) {
            *fw_size = rsp.size;
        }
        return true;
    }
    return false;
}

bool ota_read_block(uint16_t index, uint16_t block_size, uint8_t *data, uint16_t *data_len,
                    uint8_t *type) {
    at_case_packet_read req;
    req.role.des = AT_CASE_ROLE_GLASS;
    req.role.src = AT_CASE_ROLE_CASE;
    req.index = index;
    req.size = block_size;

    for (uint8_t i = 0U; i < OTA_EXCHANGE_RETRIES; i++) {
        hal_fwdgt_feed();
        uint16_t tx_len = at_frame_pack_request(ota_tx_buf, AT_OPCODE_CASE_PACKET_READ,
                                                (const uint8_t *)&req, (uint8_t)sizeof(req), 0U);
        uint16_t rx_len =
            hal_usart_send_recv(ota_tx_buf, tx_len, ota_rx_buf, OTA_BUF_SIZE, OTA_TIMEOUT_MS);
        if (rx_len == 0U) {
            continue;
        }

        uint16_t opcode = 0U;
        uint8_t status = AT_ERR_UNKNOWN;
        uint8_t plen = 0U;
        if (at_frame_parse(ota_rx_buf, rx_len, &opcode, &status, ota_payload, &plen) !=
            AT_SUCCESS) {
            continue;
        }
        if (opcode != AT_OPCODE_CASE_PACKET_READ || status != AT_SUCCESS) {
            continue;
        }
        /* The transfer header is role(2) + index(2) + type(1); data follows. */
        if (plen < (uint8_t)offsetof(at_case_packet_transfer, data)) {
            continue;
        }

        const at_case_packet_transfer *rsp = (const at_case_packet_transfer *)ota_payload;
        if (rsp->index != index) {
            continue;
        }
        uint16_t dlen = (uint16_t)(plen - (uint8_t)offsetof(at_case_packet_transfer, data));
        if (data != NULL) {
            memcpy(data, rsp->data, dlen);
        }
        if (data_len != NULL) {
            *data_len = dlen;
        }
        if (type != NULL) {
            *type = rsp->type;
        }
        return true;
    }
    return false;
}

/* Clear the OTA flag on both sides and let the state machine resume. A final
 * heartbeat with the flag cleared tells the glasses to leave OTA mode; the local
 * flag is dropped regardless of whether the glasses answers. */
static void ota_finish(sm_ctx_t *ctx) {
    ota_active = false;
    ctx->ota_requested = false;
    (void)ota_heartbeat(ctx, false, NULL);
}

int ota_run(sm_ctx_t *ctx, ota_progress_cb_t progress_cb) {
    hal_fwdgt_feed();
    if (progress_cb != NULL) {
        progress_cb(0U);
    }

    if (!ota_request(ctx)) {
        ota_finish(ctx);
        return OTA_ERR_REQUEST;
    }

    uint32_t fw_size = 0U;
    if (!ota_prepare(&fw_size)) {
        ota_finish(ctx);
        return OTA_ERR_PREPARE;
    }

    static uint8_t block[OTA_BLOCK_SIZE];
    uint32_t max_blocks = (fw_size / OTA_BLOCK_SIZE) + 2U;
    if (max_blocks > OTA_MAX_BLOCKS) {
        max_blocks = OTA_MAX_BLOCKS;
    }

    uint16_t index = 0U;
    uint32_t received = 0U;
    for (;;) {
        if (index >= max_blocks) {
            ota_finish(ctx);
            return OTA_ERR_RUNAWAY;
        }

        uint16_t dlen = 0U;
        uint8_t type = AT_PACKET_TYPE_MID;
        if (!ota_read_block(index, OTA_BLOCK_SIZE, block, &dlen, &type)) {
            ota_finish(ctx);
            return OTA_ERR_READ;
        }

        received += dlen;
        if (progress_cb != NULL && fw_size > 0U) {
            uint32_t pct = (received * 100U) / fw_size;
            if (pct > 100U) {
                pct = 100U;
            }
            progress_cb((uint8_t)pct);
        }

        index++;
        if (type == AT_PACKET_TYPE_END) {
            break;
        }
    }

    if (progress_cb != NULL) {
        progress_cb(100U);
    }
    ota_finish(ctx);
    return OTA_OK;
}
