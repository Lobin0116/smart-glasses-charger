#include "charge_flow.h"

#include "at_frame.h"
#include "at_opcode.h"
#include "at_types.h"
#include "hal_gpio.h"
#include "hal_pwr.h"
#include "hal_timer.h"
#include "hal_usart.h"

uint8_t sm_build_case_soc_byte(uint8_t soc, bool charging)
{
    return (uint8_t)((charging ? 0x80U : 0U) | (soc & 0x7FU));
}

uint8_t sm_build_case_sta_byte(bool lid_open, bool ota_requested)
{
    uint8_t sta = 0U;
    if (lid_open) {
        sta |= 0x01U;
    }
    if (ota_requested) {
        sta |= 0x80U;
    }
    return sta;
}
typedef enum
{
    PG_IDLE,
    PG_PULSE,
    PG_DISCHARGE,
    PG_COMM,
    PG_HB_SEND,
    PG_HB_WAIT,
    PG_HB_GAP,
    PG_RESTORE,
} pogo_phase_t;

static pogo_phase_t pg_phase;
static uint32_t pg_phase_start;
static uint8_t pg_hb_total;
static uint8_t pg_hb_attempts;
static bool pg_ok;
static at_glass_data pg_reply;

static void pogo_enter(pogo_phase_t phase)
{
    pg_phase = phase;
    pg_phase_start = hal_timer_get_ms();
}

static void pogo_attempt_failed(void)
{
    if (pg_hb_attempts < pg_hb_total) {
        pogo_enter(PG_HB_GAP);
    } else {
        pogo_enter(PG_RESTORE);
    }
}

static void pogo_send_heartbeat(sm_ctx_t *ctx)
{
    uint8_t buf[64];

    at_case_data payload = {
        .role = {.des = AT_CASE_ROLE_GLASS, .src = AT_CASE_ROLE_CASE},
        .case_soc = sm_build_case_soc_byte(ctx->case_soc, ctx->glass_charging),
        .case_sta = sm_build_case_sta_byte(ctx->lid_open, ctx->ota_requested),
    };

    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_HEART, (uint8_t *)&payload, sizeof(payload), 0x00);
    hal_usart_send(buf, frame_len);
}

static bool pogo_parse_reply(uint8_t *rsp, uint16_t rsp_len)
{
    uint16_t opcode;
    uint8_t status;
    uint8_t payload[64];
    uint8_t payload_len;
    if (at_frame_parse(rsp, rsp_len, &opcode, &status, payload, &payload_len) != AT_SUCCESS
        || opcode != AT_OPCODE_CASE_HEART) {
        return false;
    }

    if (payload_len >= sizeof(at_glass_data)) {
        pg_reply = *(at_glass_data *)payload;
        return true;
    }
    return false;
}

void pogo_start(sm_ctx_t *ctx, bool pulse, uint8_t hb_retries)
{
    (void)ctx;
    pg_hb_total = hb_retries;
    pg_hb_attempts = 0U;
    pg_ok = false;

    if (pulse) {

        hal_pwr_enter_charge();
        pogo_enter(PG_PULSE);
    } else {

        hal_rpd_enable();
        pogo_enter(PG_DISCHARGE);
    }
}

void pogo_tick(sm_ctx_t *ctx)
{
    switch (pg_phase) {
        case PG_IDLE:
            break;

        case PG_PULSE:
            if (!hal_timer_expired(pg_phase_start, HANDSHAKE_5V_MS)) {
                break;
            }
            hal_rpd_enable();
            pogo_enter(PG_DISCHARGE);
            break;

        case PG_DISCHARGE:
            if (!hal_timer_expired(pg_phase_start, HANDSHAKE_DISCHARGE_MS)) {
                break;
            }
            hal_rpd_disable();
            pogo_enter(PG_COMM);
            break;

        case PG_COMM:

            hal_pwr_enter_comm_switch();
            pogo_enter(PG_HB_SEND);
            break;

        case PG_HB_SEND:
            pogo_send_heartbeat(ctx);
            pg_hb_attempts++;
            pogo_enter(PG_HB_WAIT);
            break;

        case PG_HB_WAIT: {
            uint8_t rsp[64];
            uint16_t rsp_len = at_frame_try(rsp, sizeof(rsp), AT_OPCODE_CASE_HEART);
            if (rsp_len > 0U) {
                if (pogo_parse_reply(rsp, rsp_len)) {
                    pg_ok = true;
                    pogo_enter(PG_RESTORE);
                } else {

                    pogo_attempt_failed();
                }
                break;
            }
            if (!hal_timer_expired(pg_phase_start, COMM_TIMEOUT_MS)) {
                break;
            }

            hal_usart_rx_clear();
            pogo_attempt_failed();
            break;
        }

        case PG_HB_GAP:
            if (!hal_timer_expired(pg_phase_start, HANDSHAKE_RETRY_INTERVAL_MS)) {
                break;
            }
            pogo_enter(PG_HB_SEND);
            break;

        case PG_RESTORE:

            hal_pwr_enter_charge();
            pogo_enter(PG_IDLE);
            break;
    }
}

bool pogo_busy(void)
{
    return pg_phase != PG_IDLE;
}

bool pogo_done_ok(void)
{
    return pg_ok;
}

const at_glass_data *pogo_reply(void)
{
    return &pg_reply;
}

bool sm_do_shutdown(void)
{
    uint8_t buf[64];
    uint8_t rsp[64];

    at_case_role role = {.des = AT_CASE_ROLE_GLASS, .src = AT_CASE_ROLE_CASE};
    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_SHUTDOWN, (uint8_t *)&role, sizeof(role), 0x00);

    hal_usart_send(buf, frame_len);
    uint16_t rsp_len = at_frame_recv(rsp, sizeof(rsp), COMM_TIMEOUT_MS, AT_OPCODE_CASE_SHUTDOWN);
    return rsp_len > 0U;
}
