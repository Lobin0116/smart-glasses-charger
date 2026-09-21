#include <stddef.h>
#include <string.h>

#include "ota_flow.h"

#include "at_frame.h"
#include "at_opcode.h"
#include "at_types.h"
#include "gd32e23x.h"
#include "hal_bootmeta.h"
#include "hal_flash.h"
#include "hal_timer.h"
#include "hal_usart.h"
#include "hal_wwdgt.h"

#define OTA_TIMEOUT_MS 100U

#define OTA_REQUEST_RETRIES 3U
#define OTA_REQUEST_GAP_MS 100U

#define OTA_EXCHANGE_RETRIES 5U

#define OTA_BLOCK_SIZE 240U

#define OTA_MAX_BLOCKS 1024U

#define OTA_OK 0
#define OTA_ERR_REQUEST (-1)
#define OTA_ERR_PREPARE (-2)
#define OTA_ERR_READ (-3)
#define OTA_ERR_RUNAWAY (-4)
#define OTA_ERR_FLASH_ERASE (-5)
#define OTA_ERR_FLASH_PROG (-6)
#define OTA_ERR_VERIFY (-7)
#define OTA_ERR_META (-8)

static bool ota_verify(uint32_t addr, uint32_t size)
{
    (void)addr;
    (void)size;
    return true;
}

#define OTA_BUF_SIZE (AT_FRAME_HEADER_SIZE + AT_FRAME_MAX_PAYLOAD)
static uint8_t ota_tx_buf[OTA_BUF_SIZE];
static uint8_t ota_rx_buf[OTA_BUF_SIZE];
static uint8_t ota_payload[AT_FRAME_MAX_PAYLOAD];

static bool ota_active;
volatile uint16_t ota_dbg_last_rx_len = 0xFFFFU;
volatile uint16_t ota_dbg_last_index = 0xFFFFU;
volatile uint16_t ota_dbg_parse_fails = 0U;
volatile uint8_t ota_dbg_last_fail_reason = 0U;
volatile uint16_t ota_dbg_last_success_rx_len = 0xFFFFU;

static bool ota_heartbeat(sm_ctx_t *ctx, bool request_ota, bool *agreed)
{
    at_case_data req;
    req.role.des = AT_CASE_ROLE_GLASS;
    req.role.src = AT_CASE_ROLE_CASE;
    req.case_soc = (uint8_t)((ctx->case_soc & 0x7FU) | (ctx->glass_charging ? 0x80U : 0x00U));
    req.case_sta = (uint8_t)((ctx->lid_open ? 0x01U : 0x00U) | (request_ota ? 0x80U : 0x00U));

    uint16_t tx_len
        = at_frame_pack_request(ota_tx_buf, AT_OPCODE_CASE_HEART, (const uint8_t *)&req, (uint8_t)sizeof(req), 0U);
    hal_usart_send(ota_tx_buf, tx_len);
    uint16_t rx_len = at_frame_recv(ota_rx_buf, OTA_BUF_SIZE, OTA_TIMEOUT_MS, AT_OPCODE_CASE_HEART);
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
    ctx->reported_case_version = rsp->case_version;
    if (agreed != NULL) {
        *agreed = (rsp->glass_sta & 0x80U) != 0U;
    }
    return true;
}

void ota_init(void) { ota_active = false; }

bool ota_request(sm_ctx_t *ctx)
{
    for (uint8_t i = 0U; i < OTA_REQUEST_RETRIES; i++) {
        hal_wwdgt_feed();
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

bool ota_prepare(uint32_t *fw_size)
{
    at_case_role req;
    req.des = AT_CASE_ROLE_GLASS;
    req.src = AT_CASE_ROLE_CASE;

    for (uint8_t i = 0U; i < OTA_EXCHANGE_RETRIES; i++) {
        hal_wwdgt_feed();
        uint16_t tx_len = at_frame_pack_request(
            ota_tx_buf, AT_OPCODE_CASE_PACKET_PREPARE, (const uint8_t *)&req, (uint8_t)sizeof(req), 0U);
        hal_usart_send(ota_tx_buf, tx_len);
        uint16_t rx_len = at_frame_recv(ota_rx_buf, OTA_BUF_SIZE, OTA_TIMEOUT_MS, AT_OPCODE_CASE_PACKET_PREPARE);
        if (rx_len == 0U) {
            continue;
        }

        uint16_t opcode = 0U;
        uint8_t status = AT_ERR_UNKNOWN;
        uint8_t plen = 0U;
        if (at_frame_parse(ota_rx_buf, rx_len, &opcode, &status, ota_payload, &plen) != AT_SUCCESS) {
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

bool ota_read_block(uint16_t index, uint16_t block_size, uint8_t *data, uint16_t *data_len, uint8_t *type)
{
    at_case_packet_read req;
    req.role.des = AT_CASE_ROLE_GLASS;
    req.role.src = AT_CASE_ROLE_CASE;
    req.index = index;
    req.size = block_size;

    for (uint8_t i = 0U; i < OTA_EXCHANGE_RETRIES; i++) {
        hal_wwdgt_feed();
        uint16_t tx_len = at_frame_pack_request(
            ota_tx_buf, AT_OPCODE_CASE_PACKET_READ, (const uint8_t *)&req, (uint8_t)sizeof(req), 0U);
        hal_usart_send(ota_tx_buf, tx_len);
        uint16_t rx_len = at_frame_recv(ota_rx_buf, OTA_BUF_SIZE, OTA_TIMEOUT_MS, AT_OPCODE_CASE_PACKET_READ);
        ota_dbg_last_rx_len = rx_len;
        ota_dbg_last_index = index;
        if (rx_len == 0U) {
            ota_dbg_last_fail_reason = 6U;
            ota_dbg_parse_fails++;
            continue;
        }

        uint16_t opcode = 0U;
        uint8_t status = AT_ERR_UNKNOWN;
        uint8_t plen = 0U;
        at_status parse_rc = at_frame_parse(ota_rx_buf, rx_len, &opcode, &status, ota_payload, &plen);
        if (parse_rc != AT_SUCCESS) {
            ota_dbg_parse_fails++;

            if (parse_rc == AT_ERR_MAGIC) {
                ota_dbg_last_fail_reason = 1U;
            } else if (parse_rc == AT_ERR_LENGTH) {
                ota_dbg_last_fail_reason = 2U;
            } else if (parse_rc == AT_ERR_CRC) {
                ota_dbg_last_fail_reason = 3U;
            } else {
                ota_dbg_last_fail_reason = 9U;
            }
            continue;
        }
        if (opcode != AT_OPCODE_CASE_PACKET_READ || status != AT_SUCCESS) {
            ota_dbg_parse_fails++;
            ota_dbg_last_fail_reason = 4U;
            continue;
        }

        if (plen < (uint8_t)offsetof(at_case_packet_transfer, data)) {
            ota_dbg_parse_fails++;
            ota_dbg_last_fail_reason = 4U;
            continue;
        }

        const at_case_packet_transfer *rsp = (const at_case_packet_transfer *)ota_payload;
        if (rsp->index != index) {
            ota_dbg_parse_fails++;
            ota_dbg_last_fail_reason = 5U;
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
        ota_dbg_last_fail_reason = 0U;
        ota_dbg_last_success_rx_len = rx_len;
        return true;
    }
    return false;
}

static void ota_finish(sm_ctx_t *ctx)
{
    ota_active = false;
    ctx->ota_requested = false;

    ctx->reported_case_version = 0U;
    (void)ota_heartbeat(ctx, false, NULL);
}

int ota_run(sm_ctx_t *ctx, ota_progress_cb_t progress_cb)
{
    hal_wwdgt_feed();
    ota_dbg_last_fail_reason = 0U;
    if (progress_cb != NULL) {
        progress_cb(0U);
    }

    if (!ota_request(ctx)) {
        ota_dbg_last_fail_reason = 10U;
        ota_dbg_last_index = 0U;
        ota_finish(ctx);
        return OTA_ERR_REQUEST;
    }

    hal_usart_rx_clear();

    uint32_t fw_size = 0U;
    if (!ota_prepare(&fw_size)) {
        ota_dbg_last_fail_reason = 11U;
        ota_dbg_last_index = 0U;
        ota_finish(ctx);
        return OTA_ERR_PREPARE;
    }

    hal_usart_rx_clear();

    uint32_t staging_pages = BOOT_STAGING_SIZE / HAL_FLASH_PAGE_SIZE;
    uint32_t total_pages = (fw_size + HAL_FLASH_PAGE_SIZE - 1U) / HAL_FLASH_PAGE_SIZE;
    if (total_pages == 0U || total_pages > staging_pages) {
        ota_dbg_last_fail_reason = 12U;
        ota_dbg_last_index = (uint16_t)total_pages;
        ota_finish(ctx);
        return OTA_ERR_PREPARE;
    }
    hal_flash_unlock();
    for (uint32_t p = 0U; p < total_pages; p++) {
        if (!hal_flash_page_erase(BOOT_STAGING_BASE + p * HAL_FLASH_PAGE_SIZE)) {
            hal_flash_lock();
            ota_dbg_last_fail_reason = 13U;
            ota_dbg_last_index = (uint16_t)p;
            ota_finish(ctx);
            return OTA_ERR_FLASH_ERASE;
        }
    }

    static uint8_t block[OTA_BLOCK_SIZE];
    uint32_t offset = 0U;
    uint16_t index = 0U;
    uint8_t type = AT_PACKET_TYPE_MID;
    while (type != AT_PACKET_TYPE_END) {
        if (index >= OTA_MAX_BLOCKS) {
            hal_flash_lock();
            ota_dbg_last_fail_reason = 17U;
            ota_dbg_last_index = index;
            ota_finish(ctx);
            return OTA_ERR_RUNAWAY;
        }
        uint16_t dlen = 0U;
        if (!ota_read_block(index, OTA_BLOCK_SIZE, block, &dlen, &type)) {
            hal_flash_lock();

            ota_dbg_last_index = index;
            ota_finish(ctx);
            return OTA_ERR_READ;
        }
        if (dlen > 0U) {

            uint32_t padded = ((uint32_t)dlen + 3U) & ~3U;
            for (uint32_t i = dlen; i < padded; i++) {
                block[i] = 0xFFU;
            }
            if (!hal_flash_write(BOOT_STAGING_BASE + offset, block, padded)) {
                hal_flash_lock();
                ota_dbg_last_fail_reason = 15U;
                ota_dbg_last_index = index;
                ota_dbg_last_rx_len = (uint16_t)(offset + dlen);
                ota_finish(ctx);
                return OTA_ERR_FLASH_PROG;
            }
            offset += dlen;
        }
        if (progress_cb != NULL && fw_size > 0U) {
            uint32_t pct = (offset * 100U) / fw_size;
            if (pct > 100U) {
                pct = 100U;
            }
            progress_cb((uint8_t)pct);
        }
        index++;
    }
    hal_flash_lock();

    if (!ota_verify(BOOT_STAGING_BASE, offset)) {
        ota_dbg_last_fail_reason = 16U;
        ota_dbg_last_index = (uint16_t)(offset / OTA_BLOCK_SIZE);
        ota_finish(ctx);
        return OTA_ERR_VERIFY;
    }

    if (!hal_bootmeta_set_staged(offset)) {
        ota_dbg_last_fail_reason = 18U;
        ota_dbg_last_index = (uint16_t)(offset & 0xFFFFU);
        ota_finish(ctx);
        return OTA_ERR_META;
    }

    if (progress_cb != NULL) {
        progress_cb(100U);
    }
    ota_finish(ctx);

    hal_wwdgt_feed();
    NVIC_SystemReset();
    return OTA_OK;
}
