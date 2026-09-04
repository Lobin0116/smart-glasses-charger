#ifndef HAL_EXTI_H
#define HAL_EXTI_H

#include <stdint.h>

/* EXTI line numbers delivered to the application callback. Each maps 1:1 to a
 * board interrupt input of the same name; the callback dispatches on these.
 * Board1_V2: CHAGER_INT moved to PB2 and NU1671 nINT replaced the MT5706
 * COIL_INTB line. */
#define HAL_EXTI_LINE_CHARGER_INT 2U /* PB2  - wired charge IRQ (IP5353 INT) */
#define HAL_EXTI_LINE_HALL        4U /* PB4  - hall field detect             */
#define HAL_EXTI_LINE_NINT        7U /* PA7  - NU1671 wireless charge IRQ    */
#define HAL_EXTI_LINE_BAT_INT     8U /* PA8  - fuel gauge IRQ                */
#define HAL_EXTI_LINE_KEY         3U  /* PB3  - user button                   */

/* A single callback serves every EXTI line; the line number identifies the
 * source. Register once at startup; ISRs no-op while no callback is set. */
typedef void (*hal_exti_callback_t)(uint8_t line);

/* Configure the five interrupt inputs as EXTI sources and enable their NVIC
 * vectors. The GPIO pads themselves are owned by hal_gpio (pull-up inputs). */
void hal_exti_init(void);

/* Register the callback invoked from each EXTI ISR. NULL unregisters. */
void hal_exti_register_callback(hal_exti_callback_t cb);

#endif /* HAL_EXTI_H */
