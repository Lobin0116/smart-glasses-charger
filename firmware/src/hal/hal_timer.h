#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdbool.h>
#include <stdint.h>

extern volatile uint32_t hal_timer_millis;

void hal_timer_init(void);

uint32_t hal_timer_get_ms(void);

void hal_timer_delay_ms(uint32_t ms);

uint32_t hal_timer_elapsed(uint32_t start_ms);

bool hal_timer_expired(uint32_t start_ms, uint32_t timeout_ms);

#endif
