#include "hal_timer.h"

#include "gd32e23x.h"

volatile uint32_t hal_timer_millis;

void hal_timer_init(void) {
    /* SysTick_Config sizes RELOAD for a 1 ms period at the core clock, clears
     * the counter, and arms the TICKINT. It also parks SysTick at the lowest
     * NVIC priority so the EXTI sources can always preempt it. At 72 MHz the
     * 1 kHz reload (71999) fits the 24-bit field with wide margin. */
    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t hal_timer_get_ms(void) {
    return hal_timer_millis;
}

void hal_timer_delay_ms(uint32_t ms) {
    uint32_t start = hal_timer_get_ms();
    while (hal_timer_elapsed(start) < ms) {
    }
}

uint32_t hal_timer_elapsed(uint32_t start_ms) {
    return hal_timer_get_ms() - start_ms;
}

bool hal_timer_expired(uint32_t start_ms, uint32_t timeout_ms) {
    return hal_timer_elapsed(start_ms) >= timeout_ms;
}

void SysTick_Handler(void) {
    hal_timer_millis++;
}
