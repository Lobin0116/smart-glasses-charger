/*
 * Minimal mock of GD32 SPL functions for host-side unit testing.
 *
 * We intercept key SPL calls to record what the firmware tried to do,
 * without touching real hardware. The mock keeps counters, last-set values,
 * and simple state machines where needed.
 */

#include "mock_spl.h"

#include <string.h>

/* --- tracked state --- */

uint32_t mock_gpio_mode_set_calls = 0;
uint32_t mock_gpio_output_options_set_calls = 0;
uint32_t mock_gpio_af_set_calls = 0;
uint32_t mock_rcu_periph_clock_enable_calls = 0;

uint32_t mock_gpio_output_port = 0;
uint32_t mock_gpio_output_mode = 0;
uint32_t mock_gpio_output_pupd = 0;
uint32_t mock_gpio_output_pin = 0;
uint32_t mock_gpio_output_otype = 0;
uint32_t mock_gpio_output_ospeed = 0;

uint32_t mock_af_port = 0;
uint32_t mock_af_af = 0;
uint32_t mock_af_pin = 0;

uint32_t mock_rcu_clocks_enabled[64] = {0};

uint32_t mock_gpio_output_reg[3] = {0}; /* GPIOA, GPIOB, GPIOF */
uint32_t mock_gpio_input_reg[3] = {0};

/* USART mock state */
uint8_t mock_usart_tx_buf[256];
uint16_t mock_usart_tx_len = 0;
uint8_t mock_usart_rx_buf[256];
uint16_t mock_usart_rx_len = 0;
uint16_t mock_usart_rx_idx = 0;
uint8_t mock_usart_tx_ready = 1;
uint8_t mock_usart_rx_ready = 0;

void mock_reset(void) {
    mock_gpio_mode_set_calls = 0;
    mock_gpio_output_options_set_calls = 0;
    mock_gpio_af_set_calls = 0;
    mock_rcu_periph_clock_enable_calls = 0;
    memset(mock_rcu_clocks_enabled, 0, sizeof(mock_rcu_clocks_enabled));
    memset(mock_gpio_output_reg, 0, sizeof(mock_gpio_output_reg));
    memset(mock_gpio_input_reg, 0, sizeof(mock_gpio_input_reg));
    mock_usart_tx_len = 0;
    mock_usart_rx_len = 0;
    mock_usart_rx_idx = 0;
    mock_usart_tx_ready = 1;
    mock_usart_rx_ready = 0;
}

void mock_set_rx_data(const uint8_t *data, uint16_t len) {
    memcpy(mock_usart_rx_buf, data, len);
    mock_usart_rx_len = len;
    mock_usart_rx_idx = 0;
    mock_usart_rx_ready = (len > 0) ? 1 : 0;
}
