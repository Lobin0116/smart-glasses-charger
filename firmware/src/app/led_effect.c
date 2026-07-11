#include "led_effect.h"

#include "led.h"

#define BATTERY_DISPLAY_MS 7000U

static led_color_t soc_to_color(uint8_t soc) {
    if (soc > 40U) {
        return LED_WHITE;
    }
    if (soc >= 15U) {
        return LED_GREEN;
    }
    return LED_RED;
}

static void apply_effect(led_effect_id_t effect, uint8_t soc) {
    led_all_off();
    switch (effect) {
    case LED_EFFECT_CASE_CHARGING_BREATH:
    case LED_EFFECT_GLASS_CHARGING_BREATH:
        led_set(soc_to_color(soc), LED_BREATH);
        break;
    case LED_EFFECT_FULL_SOLID:
        led_set(LED_WHITE, LED_ON);
        break;
    case LED_EFFECT_BATTERY_DISPLAY:
        led_set(soc_to_color(soc), LED_ON);
        break;
    case LED_EFFECT_LOW_BATT_BLINK:
        led_set(LED_RED, LED_BLINK);
        break;
    default:
        break;
    }
}

static led_effect_id_t resolve_effect(led_effect_ctx_t *ctx) {
    if (ctx->case_full && ctx->glass_full) {
        return LED_EFFECT_FULL_SOLID;
    }
    if (ctx->case_soc < 5U) {
        return LED_EFFECT_LOW_BATT_BLINK;
    }
    if (ctx->glass_charging) {
        return LED_EFFECT_GLASS_CHARGING_BREATH;
    }
    if (ctx->case_charging) {
        return LED_EFFECT_CASE_CHARGING_BREATH;
    }
    return LED_EFFECT_NONE;
}

void led_effect_init(led_effect_ctx_t *ctx) {
    ctx->current = LED_EFFECT_NONE;
    ctx->overlay = LED_EFFECT_NONE;
    ctx->overlay_start_ms = 0U;
    ctx->overlay_duration_ms = 0U;
    ctx->case_soc = 0U;
    ctx->case_charging = false;
    ctx->glass_charging = false;
    ctx->glass_full = false;
    ctx->case_full = false;
    led_all_off();
}

void led_effect_set_case_info(led_effect_ctx_t *ctx, uint8_t soc, bool charging, bool full) {
    ctx->case_soc = soc;
    ctx->case_charging = charging;
    ctx->case_full = full;
}

void led_effect_set_glass_info(led_effect_ctx_t *ctx, bool charging, bool full) {
    ctx->glass_charging = charging;
    ctx->glass_full = full;
}

void led_effect_show_battery(led_effect_ctx_t *ctx, uint8_t soc) {
    led_effect_overlay(ctx, LED_EFFECT_BATTERY_DISPLAY, BATTERY_DISPLAY_MS);
    ctx->case_soc = soc;
}

void led_effect_overlay(led_effect_ctx_t *ctx, led_effect_id_t effect, uint32_t duration_ms) {
    ctx->overlay = effect;
    ctx->overlay_start_ms = hal_timer_get_ms();
    ctx->overlay_duration_ms = duration_ms;
}

void led_effect_poll(led_effect_ctx_t *ctx) {
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
        apply_effect(target, ctx->case_soc);
    }

    led_poll();
}
