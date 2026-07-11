#ifndef HAL_EXTI_H
#define HAL_EXTI_H

#include <stdint.h>

/* EXTI line numbers delivered to the application callback. Each maps 1:1 to a
 * board interrupt input of the same name; the callback dispatches on these. */
#define HAL_EXTI_LINE_BAT_INT 8U      /* PA8  - fuel gauge IRQ       */
#define HAL_EXTI_LINE_CHARGER_INT 11U /* PA11 - wired charge IRQ    */
#define HAL_EXTI_LINE_COIL_INT 12U    /* PA12 - wireless charge IRQ */
#define HAL_EXTI_LINE_KEY 3U          /* PB3  - user button          */
#define HAL_EXTI_LINE_HALL 4U         /* PB4  - hall field detect    */

/* A single callback serves every EXTI line; the line number identifies the
 * source. Register once at startup; ISRs no-op while no callback is set. */
typedef void (*hal_exti_callback_t)(uint8_t line);

/* Configure the five interrupt inputs as EXTI sources and enable their NVIC
 * vectors. The GPIO pads themselves are owned by hal_gpio (pull-up inputs). */
void hal_exti_init(void);

/* Register the callback invoked from each EXTI ISR. NULL unregisters. */
void hal_exti_register_callback(hal_exti_callback_t cb);

#endif /* HAL_EXTI_H */
