#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdbool.h>
#include <stdint.h>

/* Global 1 ms system tick, incremented in SysTick_Handler. Every module with a
 * timeout (handshake phases, AT request deadlines, charge timers) reads this
 * through the helpers below rather than owning its own counter. */
extern volatile uint32_t hal_timer_millis;

/* Arm SysTick for a 1 ms interrupt at SystemCoreClock and seed the global
 * counter at zero. Must run before any code calls the delay/timeout helpers. */
void hal_timer_init(void);

/* Current millisecond count. A 32-bit aligned read is atomic on Cortex-M23, so
 * the value is never torn even while the ISR advances it. */
uint32_t hal_timer_get_ms(void);

/* Spin until ms milliseconds have elapsed since entry. */
void hal_timer_delay_ms(uint32_t ms);

/* Milliseconds elapsed since start_ms was sampled. The unsigned difference
 * stays correct across the 32-bit wraparound (~49 days). */
uint32_t hal_timer_elapsed(uint32_t start_ms);

/* True once timeout_ms has elapsed since start_ms. A timeout_ms of 0 reads as
 * already expired. */
bool hal_timer_expired(uint32_t start_ms, uint32_t timeout_ms);

#endif /* HAL_TIMER_H */
