#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum
{
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_WHITE,
    LED_COLOR_COUNT,
} led_color_t;

typedef enum
{
    LED_OFF,
    LED_ON,
    LED_BREATH,
    LED_BLINK,
} led_mode_t;

void led_init(void);

void led_set(led_color_t color, led_mode_t mode);

void led_all_off(void);

void led_set_by_soc(uint8_t soc);

void led_poll(void);

void led_pwm_tick(void);

#endif
