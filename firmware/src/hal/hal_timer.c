#include "hal_timer.h"

#include "gd32e23x.h"
#include "hal_wwdgt.h"
#include "led.h"

volatile uint32_t hal_timer_millis;

void hal_timer_init(void)
{
    /* SysTick at 1 kHz backs hal_timer_get_ms() — the millisecond time base
     * used everywhere in the app. Lowest NVIC priority so EXTI can preempt. */
    SysTick_Config(SystemCoreClock / 1000U);

    /* TIMER13 dedicated to LED PWM at 10 kHz (0.1 ms tick). Decoupled from
     * SysTick so we can run PWM 10× finer than the 1 ms time base without
     * disturbing it. The 10 ms carrier (100 Hz) sliced into 100 sub-ticks
     * gives 1 % duty resolution, enough that the breath curve is continuous
     * to the eye instead of stepping through a handful of visible levels.
     *
     * prescaler = 720-1 → 72 MHz / 720 = 100 kHz counter
     * period    = 10-1  → 100 kHz / 10 = 10 kHz update IRQ
     * Priority 3 (lowest after SysTick/EXTI) so charge-poll/handshake edges
     * always preempt LED bookkeeping. */
    rcu_periph_clock_enable(RCU_TIMER13);
    timer_deinit(TIMER13);
    timer_parameter_struct tp;
    timer_struct_para_init(&tp);
    tp.prescaler = 719U;  /* 72 MHz / 720 = 100 kHz counter */
    tp.period    = 9U;    /* 100 kHz / 10 = 10 kHz update IRQ */
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
    /* LED PWM moved to TIMER13 @ 10 kHz — keeps ms time base clean. */
}

/* TIMER13 update IRQ — drives led_pwm_tick at 10 kHz. The handler does no
 * millisecond accounting; it just advances the PWM sub-tick. */
void TIMER13_IRQHandler(void)
{
    if (SET == timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);
        led_pwm_tick();
    }
}
