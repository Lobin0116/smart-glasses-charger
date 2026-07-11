#ifndef LED_H
#define LED_H

#include <stdint.h>

/* Status indicator LEDs. Today the board exposes four discrete GPIO LEDs
 * (PB8/PB9/PF6/PF7); a WS2812 on PB2 is under evaluation. The public interface
 * below is intentionally agnostic to the physical backend so the driver file
 * can be swapped without touching callers. */

typedef enum {
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_WHITE,
    LED_COLOR_COUNT,
} led_color_t;

typedef enum {
    LED_OFF,
    LED_ON,
    LED_BREATH, /* 0->100->0 brightness ramp, ~2 s cycle */
    LED_BLINK,  /* 1 Hz on/off toggle */
} led_mode_t;

/* Drive every LED off and reset effect state. */
void led_init(void);

/* Set one LED's mode. Solid modes take effect immediately; blink and breath
 * resolve on the next led_poll(). */
void led_set(led_color_t color, led_mode_t mode);

/* Turn every LED off. */
void led_all_off(void);

/* Convenience: pick a solid color from the battery state of charge and light
 * it - white (>40%), green (15-40%), red (<15%). */
void led_set_by_soc(uint8_t soc);

/* Advance breath and blink effects. Call from the main loop; timing is driven
 * by hal_timer_get_ms() so the poll cadence need not be fixed. */
void led_poll(void);

#endif /* LED_H */
