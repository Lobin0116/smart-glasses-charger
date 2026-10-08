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

/* ==== non-blocking POGO transaction engine ====
 *
 * The old flows (sm_do_handshake & co.) blocked the main loop for up to ~1.1 s
 * (5V pulse + discharge + up to 3 heartbeat waits), starving button/LED/SOC
 * work. The engine below runs the exact same wire sequence as a phase machine
 * advanced one step per pogo_tick() call — no hal_timer_delay_ms anywhere —
 * so the main loop never stalls for more than a single ~1.2 ms UART send.
 *
 * Wire timing is preserved exactly, with ONE deliberate change: the old flows
 * called hal_pwr_discharge(100) and then hal_pwr_enter_comm(), whose body ran
 * a SECOND 100 ms discharge — 200 ms total before the UART path went live.
 * The engine runs a single 100 ms discharge (the spec floor) and then the
 * no-discharge hal_pwr_enter_comm_switch(). MAINTAINING (which only ever had
 * the one internal discharge) is unchanged.
 *
 * The engine never touches sm_ctx: it reads the fields it needs to build the
 * heartbeat payload, but glass_present/glass_soc/glass_full/last_comms stay
 * owned by the state machine, which consumes pogo_reply() on completion —
 * mirroring the old split between flow and caller. */

typedef enum
{
    PG_IDLE,      /* no transaction; done flags hold the last result          */
    PG_PULSE,     /* 5V charge rail presented, waiting HANDSHAKE_5V_MS        */
    PG_DISCHARGE, /* RPD bleeding the bus, waiting HANDSHAKE_DISCHARGE_MS     */
    PG_COMM,      /* switch POGO to the UART path (no discharge, no wait)     */
    PG_HB_SEND,   /* pack + send one heartbeat request (~1.2 ms)             */
    PG_HB_WAIT,   /* poll at_frame_try until a frame lands or COMM_TIMEOUT_MS */
    PG_HB_GAP,    /* HANDSHAKE_RETRY_INTERVAL_MS pause before the next retry */
    PG_RESTORE,   /* POGO back to the 5V charge rail, latch the result        */
} pogo_phase_t;

static pogo_phase_t pg_phase;
static uint32_t pg_phase_start;
static uint8_t pg_hb_total;      /* heartbeat attempts requested at start     */
static uint8_t pg_hb_attempts;   /* attempts sent so far                      */
static bool pg_ok;               /* at least one heartbeat succeeded          */
static at_glass_data pg_reply;   /* glasses status from the successful reply  */

static void pogo_enter(pogo_phase_t phase)
{
    pg_phase = phase;
    pg_phase_start = hal_timer_get_ms();
}

/* One heartbeat attempt failed (timeout, bad frame, short payload). Retry
 * after the gap while attempts remain, otherwise finish the transaction. */
static void pogo_attempt_failed(void)
{
    if (pg_hb_attempts < pg_hb_total) {
        pogo_enter(PG_HB_GAP);
    } else {
        pogo_enter(PG_RESTORE);
    }
}

/* Build and send the heartbeat request. Byte-for-byte the payload the old
 * sm_send_heartbeat built: case_soc bit7=charging, case_sta bit0=lid,
 * bit7=OTA request. The ~1.2 ms hal_usart_send is the only "long" action in
 * the engine and is well inside the main-loop stall budget. */
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

/* Validate a received frame as a heartbeat reply carrying full glass data;
 * on success the payload is latched into pg_reply. Same checks the old
 * sm_send_heartbeat ran on its blocking recv result. */
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
    (void)ctx; /* ctx is only read when the heartbeat is sent (PG_HB_SEND) */
    pg_hb_total = hb_retries;
    pg_hb_attempts = 0U;
    pg_ok = false;

    if (pulse) {
        /* Drive the 5V charge rail for the wake pulse (handshake / force
         * probe). Same net sequence as the old hal_pwr_pulse_charge(300):
         * the discharge that follows re-sets every pin anyway. */
        hal_pwr_enter_charge();
        pogo_enter(PG_PULSE);
    } else {
        /* Charge poll / maintain heartbeat: straight to the bus bleed. */
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
            hal_rpd_enable(); /* pulse over — start bleeding the bus */
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
            /* No extra wait: the 100 ms spec-floor discharge has already run
             * in PG_DISCHARGE, so only the switch itself remains. */
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
                    /* Frame landed but is unusable (bad CRC/opcode/short
                     * payload) — count the attempt as failed like the old
                     * blocking recv did. */
                    pogo_attempt_failed();
                }
                break;
            }
            if (!hal_timer_expired(pg_phase_start, COMM_TIMEOUT_MS)) {
                break; /* still inside the reply window — keep polling */
            }
            /* Window closed with no usable reply: drop any torn residue so
             * the next attempt starts from a clean ring, mirroring
             * at_frame_recv's stage-2/6 cleanup on timeout. */
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
            /* Re-route POGO back to the 5V charge side once the heartbeat
             * burst is done. CONTEXT.md line 119 says CHARGING "供5V，周期通
             * 信" — the POGO pin must spend most of its time on the 5V rail
             * actually charging the glass, only flipping to UART for the
             * brief heartbeat window. The restore also runs on failure so the
             * next handshake attempt starts from the charge state. */
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
