#include "hal_timer.h"

#include "gd32e23x.h"
#include "hal_wwdgt.h"
#include "led.h"

volatile uint32_t hal_timer_millis;

void hal_timer_init(void)
{

    SysTick_Config(SystemCoreClock / 1000U);
    rcu_periph_clock_enable(RCU_TIMER13);
    timer_deinit(TIMER13);
    timer_parameter_struct tp;
    timer_struct_para_init(&tp);
    tp.prescaler = 719U;
    tp.period = 9U;
    timer_init(TIMER13, &tp);
    timer_interrupt_enable(TIMER13, TIMER_INT_UP);
    nvic_irq_enable(TIMER13_IRQn, 3U);
    timer_enable(TIMER13);
}

uint32_t hal_timer_get_ms(void) { return hal_timer_millis; }

void hal_timer_delay_ms(uint32_t ms)
{
    uint32_t start = hal_timer_get_ms();
    while (hal_timer_elapsed(start) < ms) {
    }
}

uint32_t hal_timer_elapsed(uint32_t start_ms) { return hal_timer_get_ms() - start_ms; }

bool hal_timer_expired(uint32_t start_ms, uint32_t timeout_ms) { return hal_timer_elapsed(start_ms) >= timeout_ms; }

void SysTick_Handler(void)
{
    hal_timer_millis++;
    hal_wwdgt_feed();

}

void TIMER13_IRQHandler(void)
{
    if (SET == timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);
        led_pwm_tick();
    }
}
