#include "mt5706.h"
#include "hal_gpio.h"

static bool mt5706_enabled;
/* Set from the EXTI ISR and read/cleared by the main loop. */
static volatile bool mt5706_event_pending;

void mt5706_enable(void) {
    hal_chip_en2_enable();
    mt5706_enabled = true;
}

void mt5706_disable(void) {
    hal_chip_en2_disable();
    mt5706_enabled = false;
}

bool mt5706_is_enabled(void) { return mt5706_enabled; }

void mt5706_on_interrupt(void) { mt5706_event_pending = true; }

bool mt5706_has_event(void) { return mt5706_event_pending; }

void mt5706_clear_event(void) { mt5706_event_pending = false; }
