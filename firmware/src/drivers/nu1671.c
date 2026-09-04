#include "nu1671.h"

#include <stddef.h>

#include "hal_gpio.h"
#include "hal_i2c.h"

/* Set from ISR context on an nINT falling edge; read/cleared by the main
 * loop. Unlike the V1 mt5706 latch, every consumer path clears it. */
static volatile bool event_pending;

/* Cache of the last bus probe. The chip only ACKs while a coil field powers
 * it, so this doubles as "case is on a wireless pad". */
static bool present;

void nu1671_on_interrupt(void) { event_pending = true; }

bool nu1671_has_event(void) { return event_pending; }

void nu1671_clear_event(void) { event_pending = false; }

void nu1671_probe(void) { present = (hal_i2c_write(NU1671_I2C_ADDR, NULL, 0U) == 0); }

bool nu1671_is_present(void) { return present; }

void nu1671_pdet_enable(bool enable) { hal_pdet_en_set(enable); }

bool nu1671_tx_pad_present(void) { return !hal_pdetb_get(); }
