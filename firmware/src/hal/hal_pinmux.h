#ifndef HAL_PINMUX_H
#define HAL_PINMUX_H

#include "gd32e23x.h"

/* Pin multiplexing for the GD32E230C8T6 smart glasses charger.
 * Every assignment below mirrors the pin table in CONTEXT.md. */

/* LEDs (push-pull output) */
#define HAL_LED_RED_PORT          GPIOB
#define HAL_LED_RED_PIN           GPIO_PIN_8
#define HAL_LED_GREEN_PORT        GPIOB
#define HAL_LED_GREEN_PIN         GPIO_PIN_9
#define HAL_LED_2812_PORT         GPIOB
#define HAL_LED_2812_PIN          GPIO_PIN_2
#define HAL_LED_BLUE_PORT         GPIOF
#define HAL_LED_BLUE_PIN          GPIO_PIN_6
#define HAL_LED_WHITE_PORT        GPIOF
#define HAL_LED_WHITE_PIN         GPIO_PIN_7

/* Control outputs (push-pull) */
#define HAL_EN_1V8_PORT           GPIOB /* 1V8EN: 1.8V LDO enable */
#define HAL_EN_1V8_PIN            GPIO_PIN_10
#define HAL_CHIP_EN2_PORT         GPIOB /* wireless charge enable */
#define HAL_CHIP_EN2_PIN          GPIO_PIN_11
#define HAL_TR_SWITCH_PORT        GPIOB /* half-duplex transceiver direction */
#define HAL_TR_SWITCH_PIN         GPIO_PIN_12
#define HAL_POGO_IN_PORT          GPIOB /* POGO_IN: ET3328 IN select */
#define HAL_POGO_IN_PIN           GPIO_PIN_13
#define HAL_SHIP_CTRL_PORT        GPIOB /* SHIP_CTR: ship-mode control */
#define HAL_SHIP_CTRL_PIN         GPIO_PIN_14
#define HAL_RPD_PORT              GPIOB /* RPD: POGO discharge enable */
#define HAL_RPD_PIN               GPIO_PIN_15

/* Interrupt inputs (input with pull-up) */
#define HAL_BAT_INT_PORT          GPIOA /* fuel gauge interrupt */
#define HAL_BAT_INT_PIN           GPIO_PIN_8
#define HAL_CHARGER_INT_PORT      GPIOA /* wired charge interrupt */
#define HAL_CHARGER_INT_PIN       GPIO_PIN_11
#define HAL_COIL_INT_PORT         GPIOA /* wireless charge interrupt */
#define HAL_COIL_INT_PIN          GPIO_PIN_12

/* User inputs (input with pull-up) */
#define HAL_KEY_PORT              GPIOB /* KEY: short-press battery check */
#define HAL_KEY_PIN               GPIO_PIN_3
#define HAL_HALL_PORT             GPIOB /* HALL_OUT_DIG: hall field detect */
#define HAL_HALL_PIN              GPIO_PIN_4

/* I2C0 (alternate function open-drain) */
#define HAL_I2C_SCL_PORT          GPIOB /* I2C0_SCL */
#define HAL_I2C_SCL_PIN           GPIO_PIN_6
#define HAL_I2C_SDA_PORT          GPIOB /* I2C0_SDA */
#define HAL_I2C_SDA_PIN           GPIO_PIN_7

/* USART0 (alternate function push-pull) */
#define HAL_UART_TX_PORT          GPIOA /* USART0_TX */
#define HAL_UART_TX_PIN           GPIO_PIN_9
#define HAL_UART_RX_PORT          GPIOA /* USART0_RX */
#define HAL_UART_RX_PIN           GPIO_PIN_10

/* ADC channels PA0..PA7 (ADC_IN0..IN7) and PB0..PB1 (ADC_IN8..IN9) are left in
 * their default analog reset state and are not handled by the GPIO HAL. */

/* Logical pin identifiers consumed by the generic GPIO helpers. */
typedef enum {
    HAL_PIN_LED_RED,       /* PB8  */
    HAL_PIN_LED_GREEN,     /* PB9  */
    HAL_PIN_LED_2812,      /* PB2  */
    HAL_PIN_LED_BLUE,      /* PF6  */
    HAL_PIN_LED_WHITE,     /* PF7  */
    HAL_PIN_EN_1V8,        /* PB10 */
    HAL_PIN_CHIP_EN2,      /* PB11 */
    HAL_PIN_TR_SWITCH,     /* PB12 */
    HAL_PIN_POGO_IN,       /* PB13 */
    HAL_PIN_SHIP_CTRL,     /* PB14 */
    HAL_PIN_RPD,           /* PB15 */
    HAL_PIN_BAT_INT,       /* PA8  */
    HAL_PIN_CHARGER_INT,   /* PA11 */
    HAL_PIN_COIL_INT,      /* PA12 */
    HAL_PIN_KEY,           /* PB3  */
    HAL_PIN_HALL,          /* PB4  */
    HAL_PIN_I2C_SCL,       /* PB6  */
    HAL_PIN_I2C_SDA,       /* PB7  */
    HAL_PIN_UART_TX,       /* PA9  */
    HAL_PIN_UART_RX,       /* PA10 */
    HAL_PIN_COUNT
} hal_pin_t;

#endif /* HAL_PINMUX_H */
