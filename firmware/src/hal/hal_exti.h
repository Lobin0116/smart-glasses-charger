#ifndef HAL_EXTI_H
#define HAL_EXTI_H

#include <stdint.h>

#define HAL_EXTI_LINE_CHARGER_INT 2U
#define HAL_EXTI_LINE_HALL 4U
#define HAL_EXTI_LINE_NINT 7U
#define HAL_EXTI_LINE_BAT_INT 8U
#define HAL_EXTI_LINE_KEY 3U

typedef void (*hal_exti_callback_t)(uint8_t line);

void hal_exti_init(void);

void hal_exti_register_callback(hal_exti_callback_t cb);

#endif
