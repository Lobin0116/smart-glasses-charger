#ifndef LED_EFFECT_H
#define LED_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_timer.h"

typedef enum
{
    LED_EFFECT_NONE,
    LED_EFFECT_CASE_CHARGING_BREATH,
    LED_EFFECT_GLASS_CHARGING_BREATH,
    LED_EFFECT_FULL_SOLID,
    LED_EFFECT_BATTERY_DISPLAY,
} led_effect_id_t;

typedef struct
{
    led_effect_id_t current;
    led_effect_id_t overlay;
    uint32_t overlay_start_ms;
    uint32_t overlay_duration_ms;
    uint8_t case_soc;
    bool case_charging;
    bool glass_charging;
    bool glass_full;
    bool case_full;
    /* Timestamp of the false→true edge of (case_full && glass_full); the
     * full-solid effect shows for LED_FULL_SOLID_MS from that edge and then
     * goes dark (REQ: "充满电，指示灯长亮7s后灭" — previously it stayed lit
     * forever, which also kept sm_can_sleep() false forever). */
    uint32_t full_solid_start_ms;
} led_effect_ctx_t;

void led_effect_init(led_effect_ctx_t *ctx);
void led_effect_set_case_info(led_effect_ctx_t *ctx, uint8_t soc, bool charging, bool full);
void led_effect_set_glass_info(led_effect_ctx_t *ctx, bool charging, bool full);
void led_effect_show_battery(led_effect_ctx_t *ctx, uint8_t soc);
void led_effect_overlay(led_effect_ctx_t *ctx, led_effect_id_t effect, uint32_t duration_ms);
void led_effect_poll(led_effect_ctx_t *ctx);

#endif
