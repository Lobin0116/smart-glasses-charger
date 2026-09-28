#include "led_effect.h"

#include "led.h"

#define BATTERY_DISPLAY_MS 7000U
/* Both-full solid display window (REQ: solid on 7s after both full, then off;
 * review 2026-09: the previous never-expiring solid also blocked Deep-Sleep
 * forever via sm_can_sleep). */
#define LED_FULL_SOLID_MS 7000U
/* Glasses-full solid display window (user decision 2026-09: with glasses on
 * the pins the LED reports the glasses, so a full glasses battery gets the
 * same treatment as both-full — solid for 7 s after the edge, then dark). */
#define LED_GLASS_FULL_MS 7000U

static led_color_t soc_to_color(uint8_t soc)
{
    if (soc > 40U) {
        return LED_WHITE;
    }
    if (soc >= 15U) {
        return LED_GREEN;
    }
    return LED_RED;
}

static void apply_effect(led_effect_id_t effect, uint8_t soc)
{
    led_all_off();
    switch (effect) {
        case LED_EFFECT_CASE_CHARGING_BREATH:
        case LED_EFFECT_GLASS_CHARGING_BREATH:
            led_set(soc_to_color(soc), LED_BREATH);
            break;
        case LED_EFFECT_GLASS_FULL:
            led_set(soc_to_color(soc), LED_ON);
            break;
        case LED_EFFECT_FULL_SOLID:
            led_set(LED_WHITE, LED_ON);
            break;
        case LED_EFFECT_BATTERY_DISPLAY:
            /* REQ §3 "电量查看": 1%<SOC≤5% 红闪 7s, >5% 对应颜色长亮 7s. */
            led_set(soc_to_color(soc), (soc <= 5U) ? LED_BLINK : LED_ON);
            break;
        default:
            break;
    }
}

static led_effect_id_t resolve_effect(led_effect_ctx_t *ctx)
{
    if (ctx->case_full && ctx->glass_full) {
        /* Time-gated: only the first LED_FULL_SOLID_MS after the both-full
         * edge lights up; afterwards it stays off while the condition holds,
         * so sleep is no longer blocked by a permanent indicator. */
        if (!hal_timer_expired(ctx->full_solid_start_ms, LED_FULL_SOLID_MS)) {
            return LED_EFFECT_FULL_SOLID;
        }
        return LED_EFFECT_NONE;
    }
    /* Display policy (user decision 2026-09): glasses present → show the
     * glasses battery; glasses absent → show the case. */
    if (ctx->glass_present) {
        if (ctx->glass_full) {
            /* Same time-gate shape as both-full, but stamped from the
             * glass_full edge alone; after the window it stays off while
             * the condition holds. */
            if (!hal_timer_expired(ctx->glass_full_start_ms, LED_GLASS_FULL_MS)) {
                return LED_EFFECT_GLASS_FULL;
            }
            return LED_EFFECT_NONE;
        }
        return LED_EFFECT_GLASS_CHARGING_BREATH;
    }
    if (ctx->case_charging) {
        return LED_EFFECT_CASE_CHARGING_BREATH;
    }
    return LED_EFFECT_NONE;
}

void led_effect_init(led_effect_ctx_t *ctx)
{
    ctx->current = LED_EFFECT_NONE;
    ctx->overlay = LED_EFFECT_NONE;
    ctx->overlay_start_ms = 0U;
    ctx->overlay_duration_ms = 0U;
    ctx->case_soc = 0U;
    ctx->case_charging = false;
    ctx->glass_full = false;
    ctx->case_full = false;
    ctx->full_solid_start_ms = 0U;
    ctx->glass_soc = 0U;
    ctx->glass_present = false;
    ctx->glass_full_start_ms = 0U;
    led_all_off();
}

void led_effect_set_case_info(led_effect_ctx_t *ctx, uint8_t soc, bool charging, bool full)
{
    bool both_before = ctx->case_full && ctx->glass_full;
    ctx->case_soc = soc;
    ctx->case_charging = charging;
    ctx->case_full = full;
    if (!both_before && ctx->case_full && ctx->glass_full) {
        ctx->full_solid_start_ms = hal_timer_get_ms();
    }
}

void led_effect_set_glass_info(led_effect_ctx_t *ctx, bool present, uint8_t soc, bool full)
{
    bool full_before = ctx->glass_full;
    bool both_before = ctx->case_full && ctx->glass_full;
    ctx->glass_present = present;
    ctx->glass_soc = soc;
    ctx->glass_full = full;
    if (!full_before && ctx->glass_full) {
        ctx->glass_full_start_ms = hal_timer_get_ms();
    }
    if (!both_before && ctx->case_full && ctx->glass_full) {
        ctx->full_solid_start_ms = hal_timer_get_ms();
    }
}

void led_effect_show_battery(led_effect_ctx_t *ctx, uint8_t soc)
{
    led_effect_overlay(ctx, LED_EFFECT_BATTERY_DISPLAY, BATTERY_DISPLAY_MS);
    ctx->case_soc = soc;
}

void led_effect_overlay(led_effect_ctx_t *ctx, led_effect_id_t effect, uint32_t duration_ms)
{
    ctx->overlay = effect;
    ctx->overlay_start_ms = hal_timer_get_ms();
    ctx->overlay_duration_ms = duration_ms;
}

void led_effect_poll(led_effect_ctx_t *ctx)
{
    led_effect_id_t target = resolve_effect(ctx);

    if (ctx->overlay != LED_EFFECT_NONE) {
        if (hal_timer_expired(ctx->overlay_start_ms, ctx->overlay_duration_ms)) {
            ctx->overlay = LED_EFFECT_NONE;
        } else {
            target = ctx->overlay;
        }
    }

    if (target != ctx->current) {
        ctx->current = target;
        /* Which battery the effect colors from: the glasses effects track the
         * glasses SOC (user decision 2026-09 display policy), everything else
         * (case breath, battery display) the case SOC. FULL_SOLID is white
         * either way; it reads the case value for symmetry. */
        uint8_t soc = ctx->case_soc;
        if (target == LED_EFFECT_GLASS_FULL || target == LED_EFFECT_GLASS_CHARGING_BREATH) {
            soc = ctx->glass_soc;
        }
        apply_effect(target, soc);
    }

    led_poll();
}
