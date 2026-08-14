#include "charge_flow.h"

#include "at_frame.h"
#include "at_opcode.h"
#include "at_types.h"
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

static bool sm_send_heartbeat(sm_ctx_t *ctx, at_glass_data *reply)
{
    uint8_t buf[64];
    uint8_t rsp[64];

    at_case_data payload = {
        .role = {.des = AT_CASE_ROLE_GLASS, .src = AT_CASE_ROLE_CASE},
        .case_soc = sm_build_case_soc_byte(ctx->case_soc, ctx->glass_charging),
        .case_sta = sm_build_case_sta_byte(ctx->lid_open, ctx->ota_requested),
    };

    uint16_t frame_len = at_frame_pack_request(buf, AT_OPCODE_CASE_HEART, (uint8_t *)&payload, sizeof(payload), 0x00);

    hal_usart_send(buf, frame_len);
    uint16_t rsp_len = at_frame_recv(rsp, sizeof(rsp), COMM_TIMEOUT_MS, AT_OPCODE_CASE_HEART);
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

bool sm_do_handshake(sm_ctx_t *ctx)
{
    hal_pwr_pulse_charge(HANDSHAKE_5V_MS);
    hal_pwr_discharge(HANDSHAKE_DISCHARGE_MS);
    hal_pwr_enter_comm();

    bool ok = false;
    for (uint8_t i = 0U; i < HANDSHAKE_RETRY_COUNT; i++) {
        at_glass_data reply;
        if (sm_send_heartbeat(ctx, &reply)) {
            ctx->glass_soc = reply.glass_soc & 0x7FU;
            ctx->glass_full = (reply.glass_soc & 0x80U) != 0U;
            ctx->glass_present = true;
            ok = true;
            break;
        }
        hal_timer_delay_ms(HANDSHAKE_RETRY_INTERVAL_MS);
    }
    if (!ok) {
        ctx->glass_present = false;
    }

    /* Re-route POGO back to the 5V charge side once the heartbeat burst is
     * done. CONTEXT.md line 119 says CHARGING "供5V，周期通信" — the POGO
     * pin must spend most of its time on the 5V rail actually charging the
     * glass, only flipping to UART for the brief heartbeat window. Without
     * this restore the previous code left ET3328 stuck on the UART side
     * after handshake, so no 5V ever reached the glass between polls. The
     * same restore runs on failure so the next handshake attempt starts
     * from the charge state. */
    hal_pwr_enter_charge();
    return ok;
}

bool sm_do_charge_poll(sm_ctx_t *ctx)
{
    hal_pwr_discharge(HANDSHAKE_DISCHARGE_MS);
    hal_pwr_enter_comm();

    at_glass_data reply;
    bool ok = false;
    if (sm_send_heartbeat(ctx, &reply)) {
        ctx->glass_soc = reply.glass_soc & 0x7FU;
        ctx->glass_full = (reply.glass_soc & 0x80U) != 0U;
        ok = true;
    }

    /* Flip POGO back to the 5V rail after the heartbeat so the glass
     * actually charges for the next ~30 s until the next poll. */
    hal_pwr_enter_charge();
    return ok;
}

bool sm_do_maintain_heartbeat(sm_ctx_t *ctx)
{
    /* MAINTAINING is "low case_soc, don't charge, just keep glass alive
     * with heartbeats". The handshake that admitted us left POGO on the
     * charge rail, so we must flip to UART for the heartbeat itself. */
    hal_pwr_enter_comm();
    at_glass_data reply;
    bool ok = sm_send_heartbeat(ctx, &reply);
    /* MAINTAINING does not charge — leave POGO parked on the charge rail
     * but rely on IP5353 / SOC-threshold gating to keep 5V off the glass. */
    hal_pwr_enter_charge();
    return ok;
}

bool sm_do_force_charge_probe(sm_ctx_t *ctx)
{
    hal_pwr_enter_charge();
    hal_timer_delay_ms(HANDSHAKE_5V_MS);
    hal_pwr_discharge(HANDSHAKE_DISCHARGE_MS);
    hal_pwr_enter_comm();

    at_glass_data reply;
    bool ok = false;
    if (sm_send_heartbeat(ctx, &reply)) {
        ctx->glass_soc = reply.glass_soc & 0x7FU;
        ctx->glass_full = (reply.glass_soc & 0x80U) != 0U;
        ctx->glass_present = true;
        ok = true;
    }

    /* FORCE_CHARGING's whole point is "long 5V supply" — restore the rail
     * after the probe so the glass keeps receiving 5V between probes. */
    hal_pwr_enter_charge();
    return ok;
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
