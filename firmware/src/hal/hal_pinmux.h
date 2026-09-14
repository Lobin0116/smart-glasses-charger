#ifndef HAL_PINMUX_H
#define HAL_PINMUX_H

#include "gd32e23x.h"

/* Pin multiplexing for the GD32E230C8T6 smart glasses charger, Board1_V2
 * (schematic SCH_Schematic1_1_2026-09-04 + its .tel netlist, authoritative).
 *
 * V2 changes vs V1: CHAGER_INT moved PA11->PB2 (PA11/PA12 are now USB D-/D+;
 * the GD32E230 has no USB peripheral, so they stay in their reset state),
 * MT5706 COIL_INTB/PA12 replaced by NU1671 nINT/PA7, PB11 CHIP_EN2 replaced
 * by the POGO3V3 power gate, PB5 added as the CH340K supply gate, PB0/PB1
 * added for NU1671 PDETB/PDET_EN, PA15 added driving the IP5353 KEY pin,
 * and the WS2812 LED on PB2 was deleted. The 12 MHz crystal is gone too —
 * firmware runs on IRC8M->PLL and never used it. */

/* LEDs (push-pull output, active-low) */
#define HAL_LED_RED_PORT   GPIOB
#define HAL_LED_RED_PIN    GPIO_PIN_8
#define HAL_LED_GREEN_PORT GPIOB
#define HAL_LED_GREEN_PIN  GPIO_PIN_9
#define HAL_LED_BLUE_PORT  GPIOF
#define HAL_LED_BLUE_PIN   GPIO_PIN_6
#define HAL_LED_WHITE_PORT GPIOF
#define HAL_LED_WHITE_PIN  GPIO_PIN_7

/* Control outputs (push-pull) */
#define HAL_EN_1V8_PORT    GPIOB /* 1V8EN: 1.8V LDO enable */
#define HAL_EN_1V8_PIN     GPIO_PIN_10
#define HAL_TR_SWITCH_PORT GPIOB /* half-duplex transceiver direction */
#define HAL_TR_SWITCH_PIN  GPIO_PIN_12
#define HAL_POGO_IN_PORT   GPIOB /* POGO_IN: ET3328 IN select */
#define HAL_POGO_IN_PIN    GPIO_PIN_13
#define HAL_SHIP_CTRL_PORT GPIOB /* SHIP_CTR: ship-mode control */
#define HAL_SHIP_CTRL_PIN  GPIO_PIN_14
#define HAL_RPD_PORT       GPIOB /* RPD: POGO discharge enable */
#define HAL_RPD_PIN        GPIO_PIN_15
#define HAL_PDET_EN_PORT   GPIOB /* PDET_EN_IN: NU1671 ping-detect enable (via Q3 to 1V8) */
#define HAL_PDET_EN_PIN    GPIO_PIN_1
#define HAL_KEY5353_PORT   GPIOA /* 5353_KEY: IP5353 KEY pin via R48 (low = pressed) */
#define HAL_KEY5353_PIN    GPIO_PIN_15

/* Module power gates: PMOS high-side switches, ACTIVE-LOW, each with a board
 * 10k pull-up to 3V3. Drive LOW to power the rail; release the pad to
 * high-impedance to cut it (the pull-up then holds the PMOS off). */
#define HAL_POGO3V3_EN_PORT       GPIOB /* POGO3V3: ET3328 + BL1551B 3V3 side */
#define HAL_POGO3V3_EN_PIN        GPIO_PIN_11
#define HAL_UART3V3_POWER_EN_PORT GPIOB /* UART3V3: CH340K supply */
#define HAL_UART3V3_POWER_EN_PIN  GPIO_PIN_5
/* Battery-rail PMOS Q4 (gate net IP5353_OUT_EN, 2.2k pull-up R4): originally
 * undriven — the top V2 hardware bug. Fly-wired to PC13 (2026-09-14 user
 * rework); same active-LOW PMOS gate pattern as the module switches. */
#define HAL_BAT_PMOS_EN_PORT      GPIOC /* BAT_EN: Q4 gate via fly-wire */
#define HAL_BAT_PMOS_EN_PIN       GPIO_PIN_13

/* Interrupt inputs */
#define HAL_BAT_INT_PORT     GPIOA /* fuel gauge interrupt, pull-up */
#define HAL_BAT_INT_PIN      GPIO_PIN_8
#define HAL_NINT_PORT        GPIOA /* NU1671 nINT, open-drain, no board pull-up -> internal one */
#define HAL_NINT_PIN         GPIO_PIN_7
#define HAL_CHARGER_INT_PORT GPIOB /* IP5353 INT via R49; board R8 33k pulls standby low */
#define HAL_CHARGER_INT_PIN  GPIO_PIN_2
#define HAL_PDETB_PORT       GPIOB /* NU1671 GP0/PDETB via Q3; no 3V3-side board pull-up */
#define HAL_PDETB_PIN        GPIO_PIN_0

/* User inputs (input with pull-up) */
#define HAL_KEY_PORT  GPIOB /* KEY: short-press battery check */
#define HAL_KEY_PIN   GPIO_PIN_3
#define HAL_HALL_PORT GPIOB /* HALL_OUT_DIG: hall field detect */
#define HAL_HALL_PIN  GPIO_PIN_4

/* I2C0 (alternate function open-drain) — NU1671 (0x34) shares this bus with
 * IP5353/CW2017 through the Q2 3V3->1V8 level shifter. */
#define HAL_I2C_SCL_PORT GPIOB
#define HAL_I2C_SCL_PIN  GPIO_PIN_6
#define HAL_I2C_SDA_PORT GPIOB
#define HAL_I2C_SDA_PIN  GPIO_PIN_7

/* USART0 (alternate function push-pull) */
#define HAL_UART_TX_PORT GPIOA
#define HAL_UART_TX_PIN  GPIO_PIN_9
#define HAL_UART_RX_PORT GPIOA
#define HAL_UART_RX_PIN  GPIO_PIN_10

/* PA11/PA12 (USB D-/D+) are left in their analog/reset state: the MCU has no
 * USB peripheral. PA5/PA6 (ADC_VBUS_SAFE / ADC_VBAT) also stay unconfigured —
 * ADC_VBAT measures the R47/C68 RC charge time rather than a divided voltage. */

/* Logical pin identifiers consumed by the generic GPIO helpers. */
typedef enum
{
    HAL_PIN_LED_RED,          /* PB8  */
    HAL_PIN_LED_GREEN,        /* PB9  */
    HAL_PIN_LED_BLUE,         /* PF6  */
    HAL_PIN_LED_WHITE,        /* PF7  */
    HAL_PIN_EN_1V8,           /* PB10 */
    HAL_PIN_TR_SWITCH,        /* PB12 */
    HAL_PIN_POGO_IN,          /* PB13 */
    HAL_PIN_SHIP_CTRL,        /* PB14 */
    HAL_PIN_RPD,              /* PB15 */
    HAL_PIN_PDET_EN,          /* PB1  */
    HAL_PIN_KEY5353,          /* PA15 */
    HAL_PIN_POGO3V3_EN,       /* PB11 */
    HAL_PIN_UART3V3_POWER_EN, /* PB5  */
    HAL_PIN_BAT_INT,          /* PA8  */
    HAL_PIN_NINT,             /* PA7  */
    HAL_PIN_CHARGER_INT,      /* PB2  */
    HAL_PIN_PDETB,            /* PB0  */
    HAL_PIN_KEY,              /* PB3  */
    HAL_PIN_HALL,             /* PB4  */
    HAL_PIN_I2C_SCL,          /* PB6  */
    HAL_PIN_I2C_SDA,          /* PB7  */
    HAL_PIN_UART_TX,          /* PA9  */
    HAL_PIN_UART_RX,          /* PA10 */
    HAL_PIN_COUNT
} hal_pin_t;

#endif /* HAL_PINMUX_H */
