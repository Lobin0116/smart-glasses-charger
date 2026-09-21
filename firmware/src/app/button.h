#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

void button_init(void);
void button_on_press(void);
void button_poll(void);
void button_set_case_soc(uint8_t soc);

bool button_is_busy(void);

#endif
