#ifndef MOCK_SPL_H
#define MOCK_SPL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * We can't include the real GD32 SPL headers (they define register addresses
 * incompatible with host). Instead we define just enough to compile the HAL
 * source against.
 */

/* Basic type stubs */
typedef uint32_t FlagStatus;
typedef uint32_t bit_status;
#define SET 1
#define RESET 0

/* GPIO port stubs */
#define GPIOA ((uint32_t)0)
#define GPIOB ((uint32_t)1)
#define GPIOF ((uint32_t)2)

/* GPIO pin stubs */
#define GPIO_PIN_0   (1u << 0)
#define GPIO_PIN_1   (1u << 1)
#define GPIO_PIN_2   (1u << 2)
#define GPIO_PIN_3   (1u << 3)
#define GPIO_PIN_4   (1u << 4)
#define GPIO_PIN_5   (1u << 5)
#define GPIO_PIN_6   (1u << 6)
#define GPIO_PIN_7   (1u << 7)
#define GPIO_PIN_8   (1u << 8)
#define GPIO_PIN_9   (1u << 9)
#define GPIO_PIN_10  (1u << 10)
#define GPIO_PIN_11  (1u << 11)
#define GPIO_PIN_12  (1u << 12)
#define GPIO_PIN_13  (1u << 13)
#define GPIO_PIN_14  (1u << 14)
#define GPIO_PIN_15  (1u << 15)

/* GPIO mode stubs */
#define GPIO_MODE_INPUT   0
#define GPIO_MODE_OUTPUT  1
#define GPIO_MODE_AF      2
#define GPIO_MODE_ANALOG  3

#define GPIO_PUPD_NONE    0
#define GPIO_PUPD_PULLUP  1
#define GPIO_PUPD_PULLDOWN 2

#define GPIO_OTYPE_PP     0
#define GPIO_OTYPE_OD     1

#define GPIO_OSPEED_2MHZ  0
#define GPIO_OSPEED_10MHZ 1
#define GPIO_OSPEED_50MHZ 2

#define GPIO_AF_0 0
#define GPIO_AF_1 1
#define GPIO_AF_2 2
#define GPIO_AF_3 3
#define GPIO_AF_4 4
#define GPIO_AF_5 5

/* RCU periph stubs */
#define RCU_GPIOA  0
#define RCU_GPIOB  1
#define RCU_GPIOF  2
#define RCU_USART0 3
#define RCU_I2C0   4
#define RCU_DMA    5

/* USART stubs */
#define USART0 ((uint32_t)0)
#define USART_MODE_TX_RX   0
#define USART_MODE_RX      1
#define USART_MODE_TX      2
#define USART_WM_IDLE      0
#define USART_PM_NONE      0
#define USART_WL_8BIT      0
#define USART_STB_1BIT     0
#define USART_FLAG_TC      0
#define USART_FLAG_RBNE    1
#define USART_INT_RBNE     0
#define USART_INT_TC       1
#define USART_INTEN_RBNE   0
#define USART_INTEN_TC     1

/* DMA stubs */
#define DMA  ((uint32_t)0)
#define DMA_CH1 0
#define DMA_CH2 1
#define DMA_CH3 2
#define DMA_CH4 3

#define DMA_DIR_PERIPHERAL_TO_MEMORY  0
#define DMA_DIR_MEMORY_TO_PERIPHERAL  1

#define DMA_MEMORY_WIDTH_8BIT   0
#define DMA_PERIPHERAL_WIDTH_8BIT 0

#define DMA_PRIORITY_LOW     0
#define DMA_PRIORITY_MEDIUM  1
#define DMA_PRIORITY_HIGH    2
#define DMA_PRIORITY_ULTRA   3

#define DMA_INT_FTF    0

/* --- Mock state externs --- */

extern uint32_t mock_gpio_mode_set_calls;
extern uint32_t mock_gpio_output_options_set_calls;
extern uint32_t mock_gpio_af_set_calls;
extern uint32_t mock_rcu_periph_clock_enable_calls;

extern uint32_t mock_af_port;
extern uint32_t mock_af_af;
extern uint32_t mock_af_pin;

extern uint32_t mock_rcu_clocks_enabled[];
extern uint32_t mock_gpio_output_reg[];
extern uint32_t mock_gpio_input_reg[];

extern uint8_t mock_usart_tx_buf[];
extern uint16_t mock_usart_tx_len;
extern uint8_t mock_usart_rx_buf[];
extern uint16_t mock_usart_rx_len;
extern uint16_t mock_usart_rx_idx;
extern uint8_t mock_usart_tx_ready;
extern uint8_t mock_usart_rx_ready;

void mock_reset(void);
void mock_set_rx_data(const uint8_t *data, uint16_t len);

/* --- SPL function stubs (in mock_spl_impl.h via macros) ---
 *
 * We implement these as inline functions so the test linker picks them up
 * instead of the real SPL objects.
 */

static inline void rcu_periph_clock_enable(uint32_t periph) {
    if (periph < 64) mock_rcu_clocks_enabled[periph] = 1;
}

static inline void rcu_periph_clock_disable(uint32_t periph) {
    if (periph < 64) mock_rcu_clocks_enabled[periph] = 0;
}

static inline void gpio_mode_set(uint32_t port, uint8_t mode, uint8_t pupd, uint32_t pin) {
    mock_gpio_mode_set_calls++;
}

static inline void gpio_output_options_set(uint32_t port, uint8_t otype, uint8_t ospeed, uint32_t pin) {
    mock_gpio_output_options_set_calls++;
}

static inline void gpio_af_set(uint32_t port, uint32_t af, uint32_t pin) {
    mock_gpio_af_set_calls++;
    mock_af_port = port;
    mock_af_af = af;
    mock_af_pin = pin;
}

static inline void gpio_bit_set(uint32_t port, uint32_t pin) {
    if (port < 3) mock_gpio_output_reg[port] |= pin;
}

static inline void gpio_bit_reset(uint32_t port, uint32_t pin) {
    if (port < 3) mock_gpio_output_reg[port] &= ~pin;
}

static inline void gpio_bit_toggle(uint32_t port, uint32_t pin) {
    if (port < 3) mock_gpio_output_reg[port] ^= pin;
}

static inline bit_status gpio_input_bit_get(uint32_t port, uint32_t pin) {
    if (port < 3 && (mock_gpio_input_reg[port] & pin))
        return SET;
    return RESET;
}

static inline void gpio_init(uint32_t port, void *cfg) {
    (void)port; (void)cfg;
}

#endif
