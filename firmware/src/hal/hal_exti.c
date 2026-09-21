#include "hal_exti.h"

#include "gd32e23x.h"

typedef struct
{
    uint8_t port;
    uint8_t pin;
    uint8_t line;
    exti_trig_type_enum trig;
} hal_exti_src_t;

static const hal_exti_src_t exti_sources[] = {

    {EXTI_SOURCE_GPIOB, EXTI_SOURCE_PIN2, HAL_EXTI_LINE_CHARGER_INT, EXTI_TRIG_BOTH},

    {EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN7, HAL_EXTI_LINE_NINT, EXTI_TRIG_FALLING},
    {EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN8, HAL_EXTI_LINE_BAT_INT, EXTI_TRIG_FALLING},
    {EXTI_SOURCE_GPIOB, EXTI_SOURCE_PIN3, HAL_EXTI_LINE_KEY, EXTI_TRIG_FALLING},
    {EXTI_SOURCE_GPIOB, EXTI_SOURCE_PIN4, HAL_EXTI_LINE_HALL, EXTI_TRIG_BOTH},
};

#define HAL_EXTI_SRC_COUNT (sizeof(exti_sources) / sizeof(exti_sources[0]))

#define HAL_EXTI_IRQ_PRIORITY 2U

static hal_exti_callback_t exti_callback;

static exti_line_enum hal_exti_mask(uint8_t line) { return (exti_line_enum)BIT(line); }

void hal_exti_init(void)
{

    rcu_periph_clock_enable(RCU_CFGCMP);

    for (uint32_t i = 0; i < HAL_EXTI_SRC_COUNT; i++) {
        const hal_exti_src_t *src = &exti_sources[i];
        exti_line_enum mask = hal_exti_mask(src->line);

        syscfg_exti_line_config(src->port, src->pin);
        exti_init(mask, EXTI_INTERRUPT, src->trig);

        exti_interrupt_flag_clear(mask);
    }

    nvic_irq_enable(EXTI2_3_IRQn, HAL_EXTI_IRQ_PRIORITY);
    nvic_irq_enable(EXTI4_15_IRQn, HAL_EXTI_IRQ_PRIORITY);
}

void hal_exti_register_callback(hal_exti_callback_t cb) { exti_callback = cb; }

static void hal_exti_dispatch(uint8_t line)
{
    exti_line_enum mask = hal_exti_mask(line);
    if (SET == exti_interrupt_flag_get(mask)) {
        exti_interrupt_flag_clear(mask);
        if (exti_callback) {
            exti_callback(line);
        }
    }
}

void EXTI2_3_IRQHandler(void)
{
    hal_exti_dispatch(HAL_EXTI_LINE_CHARGER_INT);
    hal_exti_dispatch(HAL_EXTI_LINE_KEY);
}

void EXTI4_15_IRQHandler(void)
{
    hal_exti_dispatch(HAL_EXTI_LINE_HALL);
    hal_exti_dispatch(HAL_EXTI_LINE_NINT);
    hal_exti_dispatch(HAL_EXTI_LINE_BAT_INT);
}
