/*
 * Unit tests for HAL GPIO initialization.
 *
 * Verifies that hal_gpio_init() configures the correct clocks, pin modes,
 * and AF numbers by inspecting mock call tracking.
 */

#include "test_assert.h"
#include "mock_spl.h"
#include "hal_gpio.h"

/*
 * Trick: the HAL source uses GPIOB, GPIOF etc. which are defined in mock_spl.h.
 * We re-declare hal_gpio_init so we can call it.
 */
void hal_gpio_init(void);

/* Port index mapping for checking output register state:
 * GPIOA=0, GPIOB=1, GPIOF=2 (matching mock_spl.h) */

static void test_rcu_clocks_enabled(void) {
    mock_reset();
    hal_gpio_init();

    /* All three GPIO ports must have clocks enabled */
    TEST_ASSERT(mock_rcu_clocks_enabled[RCU_GPIOA], "GPIOA clock not enabled");
    TEST_ASSERT(mock_rcu_clocks_enabled[RCU_GPIOB], "GPIOB clock not enabled");
    TEST_ASSERT(mock_rcu_clocks_enabled[RCU_GPIOF], "GPIOF clock not enabled");
}

static void test_gpio_mode_calls(void) {
    mock_reset();
    hal_gpio_init();

    /* Should have configured GPIO mode for outputs, inputs, AF pins */
    TEST_ASSERT(mock_gpio_mode_set_calls >= 6,
                "Expected at least 6 gpio_mode_set calls");
    TEST_ASSERT(mock_gpio_output_options_set_calls >= 4,
                "Expected at least 4 gpio_output_options_set calls");
}

static void test_af_configured(void) {
    mock_reset();
    hal_gpio_init();

    /* AF is called in batches: PB6|PB7 (I2C) = 1 call, PA9|PA10 (UART) = 1 call */
    TEST_ASSERT_EQ(mock_gpio_af_set_calls, 2,
                   "Expected 2 gpio_af_set calls (1 I2C batch + 1 UART batch)");
    /* Verify last AF was AF1 (USART0 or I2C0 both use AF1) */
    TEST_ASSERT_EQ(mock_af_af, GPIO_AF_1, "Last AF should be AF1");
}

static void test_leds_initially_low(void) {
    mock_reset();
    hal_gpio_init();

    /* LEDR=PB8, LEDG=PB9, LED_2812=PB2 on GPIOB(index 1) */
    uint32_t pb = mock_gpio_output_reg[1];
    TEST_ASSERT(!(pb & GPIO_PIN_8), "LEDR not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_9), "LEDG not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_2), "LED_2812 not initially low");

    /* LEDB=PF6, LEDW=PF7 on GPIOF(index 2) */
    uint32_t pf = mock_gpio_output_reg[2];
    TEST_ASSERT(!(pf & GPIO_PIN_6), "LEDB not initially low");
    TEST_ASSERT(!(pf & GPIO_PIN_7), "LEDW not initially low");
}

static void test_control_outputs_initially_low(void) {
    mock_reset();
    hal_gpio_init();

    uint32_t pb = mock_gpio_output_reg[1];
    /* 1V8EN=PB10, CHIP_EN2=PB11, T/R_SWITCH=PB12, POGO_IN=PB13,
     * SHIP_CTR=PB14, RPD=PB15 */
    TEST_ASSERT(!(pb & GPIO_PIN_10), "1V8EN not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_11), "CHIP_EN2 not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_12), "T/R_SWITCH not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_13), "POGO_IN not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_14), "SHIP_CTR not initially low");
    TEST_ASSERT(!(pb & GPIO_PIN_15), "RPD not initially low");
}

static void test_led_toggle(void) {
    mock_reset();
    hal_gpio_init();

    /* Toggle red LED on/off */
    hal_led_red_on();
    TEST_ASSERT(mock_gpio_output_reg[1] & GPIO_PIN_8,
                "LEDR should be high after on()");

    hal_led_red_off();
    TEST_ASSERT(!(mock_gpio_output_reg[1] & GPIO_PIN_8),
                "LEDR should be low after off()");
}

static void test_key_active_low(void) {
    mock_reset();
    hal_gpio_init();

    /* Key not pressed: input high (pull-up) → hal_key_pressed returns false */
    mock_gpio_input_reg[1] |= GPIO_PIN_3; /* PB3 high */
    TEST_ASSERT(!hal_key_pressed(), "Key should not be pressed when high");

    /* Key pressed: input low */
    mock_gpio_input_reg[1] &= ~GPIO_PIN_3; /* PB3 low */
    TEST_ASSERT(hal_key_pressed(), "Key should be pressed when low");
}

static void test_1v8_enable_disable(void) {
    mock_reset();
    hal_gpio_init();

    hal_1v8_enable();
    TEST_ASSERT(mock_gpio_output_reg[1] & GPIO_PIN_10,
                "1V8EN should be high after enable");

    hal_1v8_disable();
    TEST_ASSERT(!(mock_gpio_output_reg[1] & GPIO_PIN_10),
                "1V8EN should be low after disable");
}

int main(void) {
    printf("=== HAL GPIO Unit Tests ===\n\n");
    TEST_RUN(test_rcu_clocks_enabled);
    TEST_RUN(test_gpio_mode_calls);
    TEST_RUN(test_af_configured);
    TEST_RUN(test_leds_initially_low);
    TEST_RUN(test_control_outputs_initially_low);
    TEST_RUN(test_led_toggle);
    TEST_RUN(test_key_active_low);
    TEST_RUN(test_1v8_enable_disable);
    TEST_SUMMARY();
}
