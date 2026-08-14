#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

void button_init(void);
void button_on_press(void);
void button_poll(void);
void button_set_case_soc(uint8_t soc);

/* True while a press is being tracked (debounce or held). Callers (e.g. the
 * state machine deciding whether to enter Deep-Sleep) use this to stay awake
 * until the press resolves — otherwise the debounce timer freezes and the
 * short-press → battery-display path never fires. */
bool button_is_busy(void);

#endif
