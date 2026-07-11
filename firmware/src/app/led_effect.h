#ifndef LED_EFFECT_H
#define LED_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_timer.h"

typedef enum {
    LED_EFFECT_NONE,
    LED_EFFECT_CASE_CHARGING_BREATH,
    LED_EFFECT_GLASS_CHARGING_BREATH,
    LED_EFFECT_FULL_SOLID,
    LED_EFFECT_BATTERY_DISPLAY,
    LED_EFFECT_LOW_BATT_BLINK,
} led_effect_id_t;

typedef struct {
    led_effect_id_t current;
    led_effect_id_t overlay;
    uint32_t overlay_start_ms;
    uint32_t overlay_duration_ms;
    uint8_t case_soc;
    bool case_charging;
    bool glass_charging;
    bool glass_full;
    bool case_full;
} led_effect_ctx_t;

void led_effect_init(led_effect_ctx_t *ctx);
void led_effect_set_case_info(led_effect_ctx_t *ctx, uint8_t soc, bool charging, bool full);
void led_effect_set_glass_info(led_effect_ctx_t *ctx, bool charging, bool full);
void led_effect_show_battery(led_effect_ctx_t *ctx, uint8_t soc);
void led_effect_overlay(led_effect_ctx_t *ctx, led_effect_id_t effect, uint32_t duration_ms);
void led_effect_poll(led_effect_ctx_t *ctx);

#endif
