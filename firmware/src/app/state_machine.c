#include "state_machine.h"

#include <stddef.h>

#include "aux_logic.h"
#include "charge_flow.h"
#include "fw_version.h"
#include "hal_exti.h"
#include "hal_gpio.h"
#include "hal_pwr.h"
#include "hal_timer.h"
#include "led_effect.h"
#include "button.h"
#include "ota_flow.h"
#include "power_mgmt.h"
#define SM_HANDSHAKE_5V_PULSE_MS 300U
#define SM_HANDSHAKE_DISCHARGE_MS 100U
#define SM_HANDSHAKE_HB_GAP_MS 100U
#define SM_HANDSHAKE_HB_RETRIES 3U
#define SM_HANDSHAKE_TIMEOUT_MS 30000U

#define SM_MAINTAIN_HB_MS 1000U

#define SM_MAINTAIN_IDLE_TIMEOUT_MS 30000U

#define SM_CHARGE_POLL_OPEN_MS 30000U
#define SM_CHARGE_POLL_CLOSED_MS 60000U

#define SM_FORCE_PROBE_GAP_MS (3U * 60U * 1000U)
#define SM_FORCE_TIMEOUT_MS (9U * 60U * 1000U)

#define SM_SHUTDOWN_GAP_MS 100U
#define SM_SHUTDOWN_RETRIES 5U

#define SM_LOW_SOC_PCT 15U

#define SM_HANDSHAKE_ATTEMPT_GAP_MS \
    (SM_HANDSHAKE_5V_PULSE_MS + SM_HANDSHAKE_DISCHARGE_MS + (SM_HANDSHAKE_HB_GAP_MS * (SM_HANDSHAKE_HB_RETRIES - 1U)))

static uint32_t sm_last_action_ms;

static sm_state_t sm_prev_state;

static void sm_enter_state(sm_ctx_t *ctx, sm_state_t next)
{
    if (next == ST_OTA) {
        sm_prev_state = ctx->state;
    }
    ctx->state = next;
    ctx->state_enter_ms = hal_timer_get_ms();
    ctx->retry_count = 0U;
    sm_last_action_ms = ctx->state_enter_ms;
}

static void sm_goto_idle(sm_ctx_t *ctx)
{
    if (ctx->glass_present && ctx->case_soc <= SM_LOW_SOC_PCT) {
        sm_do_shutdown();
    }

    ctx->saw_glass_once = false;
    hal_pwr_idle();
    sm_enter_state(ctx, ST_IDLE);
}

static void sm_tick_handshaking(sm_ctx_t *ctx, uint32_t now)
{
    if (!hal_timer_expired(sm_last_action_ms, SM_HANDSHAKE_ATTEMPT_GAP_MS)) {
        return;
    }
    sm_last_action_ms = now;

    if (sm_do_handshake(ctx)) {
        ctx->glass_present = true;
        ctx->saw_glass_once = true;
        ctx->last_comms_ms = now;
        sm_enter_state(ctx, ctx->case_soc > SM_LOW_SOC_PCT ? ST_CHARGING : ST_MAINTAINING);
        return;
    }

    if (hal_timer_expired(ctx->state_enter_ms, SM_HANDSHAKE_TIMEOUT_MS)) {

        if (ctx->case_soc > SM_LOW_SOC_PCT && ctx->saw_glass_once) {
            sm_enter_state(ctx, ST_FORCE_CHARGING);
        } else {
            sm_goto_idle(ctx);
        }
    }
}

static void sm_tick_charging(sm_ctx_t *ctx, uint32_t now)
{

    if (ctx->reported_case_version != CASE_FW_VERSION && ctx->reported_case_version != 0U) {
        ctx->ota_requested = true;
    }
    if (ctx->ota_requested) {
        sm_enter_state(ctx, ST_OTA);
        return;
    }
    if (ctx->glass_full) {
        sm_enter_state(ctx, ctx->lid_open ? ST_MAINTAINING : ST_SHUTTING_DOWN);
        return;
    }

    ntc_zone_t zone = ntc_get_zone(ctx->ntc_temp_c);
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

static void sm_tick_maintaining(sm_ctx_t *ctx, uint32_t now)
{
    if (ctx->reported_case_version != CASE_FW_VERSION && ctx->reported_case_version != 0U) {
        ctx->ota_requested = true;
    }
    if (ctx->ota_requested) {
        sm_enter_state(ctx, ST_OTA);
        return;
    }

    if (ctx->case_soc > SM_LOW_SOC_PCT && recharge_check(ctx->glass_soc, ctx->glass_full)) {
        sm_enter_state(ctx, ST_CHARGING);
        return;
    }

    if (ctx->glass_full && hal_timer_expired(ctx->state_enter_ms, SM_MAINTAIN_IDLE_TIMEOUT_MS)) {
        sm_goto_idle(ctx);
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

static void sm_tick_force_charging(sm_ctx_t *ctx, uint32_t now)
{

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
        ctx->saw_glass_once = true;
        ctx->last_comms_ms = now;
        sm_enter_state(ctx, ST_CHARGING);
    }
}

static void sm_tick_shutting_down(sm_ctx_t *ctx, uint32_t now)
{

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

static void sm_tick_ota(sm_ctx_t *ctx, uint32_t now)
{
    (void)now;
    if (ctx->retry_count == 0U) {
        ctx->retry_count = 1U;
        ota_init();
        ota_run(ctx, NULL);
        ctx->ota_requested = false;
        sm_enter_state(ctx, sm_prev_state);
    }
}

bool sm_can_sleep(const sm_ctx_t *ctx)
{
#ifdef HIL_TEST
    return false;
#else
    if (ctx->state != ST_IDLE) {
        return false;
    }
    if (g_led_ctx.case_charging
        || g_led_ctx.glass_charging
        || g_led_ctx.overlay != LED_EFFECT_NONE
        || g_led_ctx.current == LED_EFFECT_FULL_SOLID
        || button_is_busy()) {
        return false;
    }
    return true;
#endif
}

void sm_init(sm_ctx_t *ctx)
{
    uint32_t now = hal_timer_get_ms();

    ctx->state = ST_IDLE;
    ctx->state_enter_ms = now;
    ctx->last_comms_ms = now;
    ctx->retry_count = 0U;
    ctx->glass_present = false;
    ctx->glass_soc = 0U;
    ctx->case_soc = 0U;
    ctx->glass_charging = false;
    ctx->glass_full = false;
    ctx->ota_requested = false;
    ctx->reported_case_version = 0U;
    ctx->ntc_temp_c = 25;

    sm_last_action_ms = now;
    sm_prev_state = ST_IDLE;

    ctx->lid_open = false;
    ctx->hall_edge_seen = false;
    ctx->saw_glass_once = false;
}

void sm_tick(sm_ctx_t *ctx)
{
    uint32_t now = hal_timer_get_ms();
    bool now_open = hal_hall_get();
    if (ctx->hall_edge_seen || now_open != ctx->lid_open) {
        ctx->hall_edge_seen = false;
        ctx->lid_open = now_open;

        hal_hall_pull_sync();
        if (now_open) {
            if (ctx->state == ST_IDLE) {
                sm_enter_state(ctx, ST_HANDSHAKING);
            }
            led_effect_show_battery(&g_led_ctx, ctx->case_soc);
        } else {

            if ((ctx->state == ST_CHARGING || ctx->state == ST_MAINTAINING)
                && ctx->glass_present) {
                sm_enter_state(ctx, ST_HANDSHAKING);
            } else {
                led_effect_show_battery(&g_led_ctx, ctx->case_soc);
            }
        }
    }

    switch (ctx->state) {
        case ST_IDLE:
#ifdef HIL_TEST
            break;
#else
            if (g_led_ctx.case_charging
                || g_led_ctx.glass_charging
                || g_led_ctx.overlay != LED_EFFECT_NONE
                || g_led_ctx.current == LED_EFFECT_FULL_SOLID
                || button_is_busy()) {
                break;
            }
            break;
#endif
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

            break;
    }
}

const char *sm_state_name(sm_state_t state)
{
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
