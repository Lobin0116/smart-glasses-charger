#include "charge_flow.h"

#include "at_frame.h"
#include "at_opcode.h"
#include "at_types.h"
#include "hal_pwr.h"
#include "hal_timer.h"
#include "hal_usart.h"

uint8_t sm_build_case_soc_byte(uint8_t soc, bool charging) {
    return (uint8_t)((charging ? 0x80U : 0U) | (soc & 0x7FU));
}

uint8_t sm_build_case_sta_byte(bool lid_open, bool ota_requested) {
    uint8_t sta = 0U;
    if (lid_open) {
        sta |= 0x01U;
    }
    if (ota_requested) {
        sta |= 0x80U;
    }
    return sta;
}

static bool sm_send_heartbeat(sm_ctx_t *ctx, at_glass_data *reply) {
    uint8_t buf[64];
    uint8_t rsp[64];

    at_case_data payload = {
        .role = {.des = AT_CASE_ROLE_GLASS, .src = AT_CASE_ROLE_CASE},
        .case_soc = sm_build_case_soc_byte(ctx->case_soc, ctx->glass_charging),
        .case_sta = sm_build_case_sta_byte(ctx->lid_open, ctx->ota_requested),
    };

    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_HEART, (uint8_t *)&payload,
                                               sizeof(payload), 0x00);

    uint16_t rsp_len = hal_usart_send_recv(buf, frame_len, rsp, sizeof(rsp), COMM_TIMEOUT_MS);
    if (rsp_len == 0U) {
        return false;
    }

    uint16_t opcode;
    uint8_t status;
    uint8_t rsp_payload[64];
    uint8_t rsp_payload_len;
    int rc = at_frame_parse(rsp, rsp_len, &opcode, &status, rsp_payload, &rsp_payload_len);
    if (rc != AT_SUCCESS || opcode != AT_OPCODE_CASE_HEART) {
        return false;
    }

    if (rsp_payload_len >= sizeof(at_glass_data)) {
        at_glass_data *gd = (at_glass_data *)rsp_payload;
        *reply = *gd;
        return true;
    }
    return false;
}

bool sm_do_handshake(sm_ctx_t *ctx) {
    hal_pwr_pulse_charge(HANDSHAKE_5V_MS);
    hal_pwr_discharge(HANDSHAKE_DISCHARGE_MS);
    hal_pwr_enter_comm();

    for (uint8_t i = 0; i < HANDSHAKE_RETRY_COUNT; i++) {
        at_glass_data reply;
        if (sm_send_heartbeat(ctx, &reply)) {
            ctx->glass_soc = reply.glass_soc & 0x7FU;
            ctx->glass_full = (reply.glass_soc & 0x80U) != 0U;
            ctx->glass_present = true;
            return true;
        }
        hal_timer_delay_ms(HANDSHAKE_RETRY_INTERVAL_MS);
    }

    ctx->glass_present = false;
    return false;
}

bool sm_do_charge_poll(sm_ctx_t *ctx) {
    hal_pwr_discharge(HANDSHAKE_DISCHARGE_MS);
    hal_pwr_enter_comm();

    at_glass_data reply;
    if (sm_send_heartbeat(ctx, &reply)) {
        ctx->glass_soc = reply.glass_soc & 0x7FU;
        ctx->glass_full = (reply.glass_soc & 0x80U) != 0U;
        return true;
    }
    return false;
}

bool sm_do_maintain_heartbeat(sm_ctx_t *ctx) {
    at_glass_data reply;
    return sm_send_heartbeat(ctx, &reply);
}

bool sm_do_force_charge_probe(sm_ctx_t *ctx) {
    hal_pwr_enter_charge();
    hal_timer_delay_ms(HANDSHAKE_5V_MS);
    hal_pwr_discharge(HANDSHAKE_DISCHARGE_MS);
    hal_pwr_enter_comm();

    at_glass_data reply;
    if (sm_send_heartbeat(ctx, &reply)) {
        ctx->glass_soc = reply.glass_soc & 0x7FU;
        ctx->glass_full = (reply.glass_soc & 0x80U) != 0U;
        ctx->glass_present = true;
        return true;
    }
    return false;
}

bool sm_do_shutdown(void) {
    uint8_t buf[64];
    uint8_t rsp[64];

    at_case_role role = {.des = AT_CASE_ROLE_GLASS, .src = AT_CASE_ROLE_CASE};
    uint16_t frame_len =
        at_frame_pack_request(buf, AT_OPCODE_CASE_SHUTDOWN, (uint8_t *)&role, sizeof(role), 0x00);

    uint16_t rsp_len = hal_usart_send_recv(buf, frame_len, rsp, sizeof(rsp), COMM_TIMEOUT_MS);
    return rsp_len > 0U;
}
