#include "state_machine.h"

#include <stddef.h>

#include "aux_logic.h"
#include "charge_flow.h"
#include "cw2017.h"
#include "hal_exti.h"
#include "hal_gpio.h"
#include "hal_pwr.h"
#include "hal_timer.h"
#include "led_effect.h"
#include "ota_flow.h"
#include "power_mgmt.h"

/* Timing budget from CONTEXT.md "关键时序参数汇总". All values in ms. */
#define SM_HANDSHAKE_5V_PULSE_MS 300U  /* 5V wake pulse length              */
#define SM_HANDSHAKE_DISCHARGE_MS 100U /* bus bleed before UART             */
#define SM_HANDSHAKE_HB_GAP_MS 100U    /* gap between heartbeat retries     */
#define SM_HANDSHAKE_HB_RETRIES 3U     /* heartbeat attempts per handshake  */
#define SM_HANDSHAKE_TIMEOUT_MS 30000U /* give up window before force charge */

#define SM_MAINTAIN_HB_MS 1000U /* <1.2s keeps the glass present    */

#define SM_CHARGE_POLL_OPEN_MS 30000U   /* heartbeat period, lid open     */
#define SM_CHARGE_POLL_CLOSED_MS 60000U /* heartbeat period, lid shut     */

#define SM_FORCE_PROBE_GAP_MS (3U * 60U * 1000U) /* probe cadence        */
#define SM_FORCE_TIMEOUT_MS (9U * 60U * 1000U)   /* total force window   */

#define SM_SHUTDOWN_GAP_MS 100U /* AT request/response deadline      */
#define SM_SHUTDOWN_RETRIES 5U

#define SM_LOW_SOC_PCT 15U /* charge vs maintain threshold       */

/* One full handshake attempt covers the 5V pulse, the discharge, and the gaps
 * between retries (the final retry needs no trailing gap). Pacing sm_tick() at
 * this interval keeps attempts from piling up back-to-back. */
#define SM_HANDSHAKE_ATTEMPT_GAP_MS                                                                \
    (SM_HANDSHAKE_5V_PULSE_MS + SM_HANDSHAKE_DISCHARGE_MS +                                        \
     (SM_HANDSHAKE_HB_GAP_MS * (SM_HANDSHAKE_HB_RETRIES - 1U)))

/* Timestamp of the last paced action within the current state. Reset on every
 * transition so each state paces its first action from its own entry. */
static uint32_t sm_last_action_ms;

/* State to resume when OTA completes. */
static sm_state_t sm_prev_state;

static void sm_enter_state(sm_ctx_t *ctx, sm_state_t next) {
    if (next == ST_OTA) {
        sm_prev_state = ctx->state;
    }
    ctx->state = next;
    ctx->state_enter_ms = hal_timer_get_ms();
    ctx->retry_count = 0U;
    sm_last_action_ms = ctx->state_enter_ms;
}

/* Hardware actions are implemented in charge_flow.c. */

/* Low-battery path: before sleeping, tell the glasses to shut down. */
static void sm_goto_idle(sm_ctx_t *ctx) {
    if (ctx->glass_present && ctx->case_soc <= SM_LOW_SOC_PCT) {
        sm_do_shutdown();
    }
    hal_pwr_idle();
    sm_enter_state(ctx, ST_IDLE);
}

static void sm_tick_handshaking(sm_ctx_t *ctx, uint32_t now) {
    if (!hal_timer_expired(sm_last_action_ms, SM_HANDSHAKE_ATTEMPT_GAP_MS)) {
        return;
    }
    sm_last_action_ms = now;

    if (sm_do_handshake(ctx)) {
        ctx->glass_present = true;
        ctx->last_comms_ms = now;
        sm_enter_state(ctx, ctx->case_soc > SM_LOW_SOC_PCT ? ST_CHARGING : ST_MAINTAINING);
        return;
    }

    if (hal_timer_expired(ctx->state_enter_ms, SM_HANDSHAKE_TIMEOUT_MS)) {
        if (ctx->case_soc > SM_LOW_SOC_PCT) {
            sm_enter_state(ctx, ST_FORCE_CHARGING);
        } else {
            sm_goto_idle(ctx);
        }
    }
}

static void sm_tick_charging(sm_ctx_t *ctx, uint32_t now) {
    if (ctx->ota_requested) {
        sm_enter_state(ctx, ST_OTA);
        return;
    }
    if (ctx->glass_full) {
        sm_enter_state(ctx, ctx->lid_open ? ST_MAINTAINING : ST_SHUTTING_DOWN);
        return;
    }

    /* NTC temperature protection: stop charging on critical, no action on normal. */
    ntc_zone_t zone = ntc_get_zone(cw2017_get_temp_c());
    if (ntc_should_stop_charge(zone)) {
        hal_pwr_idle();
        return;
    }

    uint32_t gap = ctx->lid_open ? SM_CHARGE_POLL_OPEN_MS : SM_CHARGE_POLL_CLOSED_MS;
    if (!hal_timer_expired(sm_last_action_ms, gap)) {
        return;
    }
    sm_last_action_ms = now;

    if (sm_do_charge_poll(ctx)) {
        ctx->last_comms_ms = now;
    }
}

static void sm_tick_maintaining(sm_ctx_t *ctx, uint32_t now) {
    if (ctx->ota_requested) {
        sm_enter_state(ctx, ST_OTA);
        return;
    }

    /* Recharge: if case has enough power and glass dropped below threshold. */
    if (ctx->case_soc > SM_LOW_SOC_PCT && recharge_check(ctx->glass_soc, ctx->glass_full)) {
        sm_enter_state(ctx, ST_CHARGING);
        return;
    }

    if (!hal_timer_expired(sm_last_action_ms, SM_MAINTAIN_HB_MS)) {
        return;
    }
    sm_last_action_ms = now;

    if (sm_do_maintain_heartbeat(ctx)) {
        ctx->last_comms_ms = now;
    }
}

static void sm_tick_force_charging(sm_ctx_t *ctx, uint32_t now) {
    /* After the 9-minute window, give up and go back to sleep. */
    if (hal_timer_expired(ctx->state_enter_ms, SM_FORCE_TIMEOUT_MS)) {
        sm_goto_idle(ctx);
        return;
    }
    if (!hal_timer_expired(sm_last_action_ms, SM_FORCE_PROBE_GAP_MS)) {
        return;
    }
    sm_last_action_ms = now;

    if (sm_do_force_charge_probe(ctx)) {
        ctx->glass_present = true;
        ctx->last_comms_ms = now;
        sm_enter_state(ctx, ST_CHARGING);
    }
}

static void sm_tick_shutting_down(sm_ctx_t *ctx, uint32_t now) {
    /* Retry a bounded number of times; no reply is read as "already off" and we
     * proceed to sleep regardless. */
    if (ctx->retry_count >= SM_SHUTDOWN_RETRIES) {
        sm_goto_idle(ctx);
        return;
    }
    if (!hal_timer_expired(sm_last_action_ms, SM_SHUTDOWN_GAP_MS)) {
        return;
    }
    sm_last_action_ms = now;

    ctx->retry_count++;
    sm_do_shutdown();
}

static void sm_tick_ota(sm_ctx_t *ctx, uint32_t now) {
    (void)now;
    if (ctx->retry_count == 0U) {
        ctx->retry_count = 1U;
        ota_init();
        ota_run(ctx, NULL);
        ctx->ota_requested = false;
        sm_enter_state(ctx, sm_prev_state);
    }
}

void sm_init(sm_ctx_t *ctx) {
    uint32_t now = hal_timer_get_ms();

    ctx->state = ST_IDLE;
    ctx->state_enter_ms = now;
    ctx->last_comms_ms = now;
    ctx->retry_count = 0U;
    ctx->lid_open = false;
    ctx->glass_present = false;
    ctx->glass_soc = 0U;
    ctx->case_soc = 0U;
    ctx->glass_charging = false;
    ctx->glass_full = false;
    ctx->ota_requested = false;

    sm_last_action_ms = now;
    sm_prev_state = ST_IDLE;
}

void sm_tick(sm_ctx_t *ctx) {
    uint32_t now = hal_timer_get_ms();

    switch (ctx->state) {
    case ST_IDLE:
        pm_enter_deep_sleep();
        break;
    case ST_HANDSHAKING:
        sm_tick_handshaking(ctx, now);
        break;
    case ST_CHARGING:
        sm_tick_charging(ctx, now);
        break;
    case ST_MAINTAINING:
        sm_tick_maintaining(ctx, now);
        break;
    case ST_FORCE_CHARGING:
        sm_tick_force_charging(ctx, now);
        break;
    case ST_SHUTTING_DOWN:
        sm_tick_shutting_down(ctx, now);
        break;
    case ST_OTA:
        sm_tick_ota(ctx, now);
        break;
    case ST_SHIP_MODE:
        /* Standby; only NRST can wake the part. No polling. */
        break;
    }
}

void sm_handle_event(sm_ctx_t *ctx, uint8_t exti_line) {
    switch (exti_line) {
    case HAL_EXTI_LINE_HALL:
        ctx->lid_open = hal_hall_get();
        if (ctx->lid_open) {
            if (ctx->state == ST_IDLE) {
                sm_enter_state(ctx, ST_HANDSHAKING);
            }
            led_effect_show_battery(&g_led_ctx, ctx->case_soc);
        } else {
            if (ctx->state == ST_CHARGING || ctx->state == ST_MAINTAINING) {
                if (ctx->glass_present) {
                    sm_enter_state(ctx, ST_HANDSHAKING);
                } else {
                    led_effect_show_battery(&g_led_ctx, ctx->case_soc);
                }
            } else if (ctx->state == ST_IDLE) {
                led_effect_show_battery(&g_led_ctx, ctx->case_soc);
            }
        }
        break;
    case HAL_EXTI_LINE_KEY:
        break;
    default:
        break;
    }
}

const char *sm_state_name(sm_state_t state) {
    switch (state) {
    case ST_IDLE:
        return "IDLE";
    case ST_HANDSHAKING:
        return "HANDSHAKING";
    case ST_CHARGING:
        return "CHARGING";
    case ST_MAINTAINING:
        return "MAINTAINING";
    case ST_FORCE_CHARGING:
        return "FORCE_CHARGING";
    case ST_SHUTTING_DOWN:
        return "SHUTTING_DOWN";
    case ST_OTA:
        return "OTA";
    case ST_SHIP_MODE:
        return "SHIP_MODE";
    default:
        return "UNKNOWN";
    }
}
