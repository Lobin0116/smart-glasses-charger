#include "nu1671.h"

#include <stddef.h>

#include "hal_gpio.h"
#include "hal_i2c.h"

static volatile bool event_pending;

static bool present;

void nu1671_on_interrupt(void) { event_pending = true; }

bool nu1671_poll(void)
{
    if (event_pending) {
        event_pending = false;
        nu1671_probe();
    }
    return present;
}

void nu1671_probe(void) { present = (hal_i2c_write(NU1671_I2C_ADDR, NULL, 0U) == 0); }

bool nu1671_is_present(void) { return present; }

void nu1671_pdet_enable(bool enable) { hal_pdet_en_set(enable); }

bool nu1671_tx_pad_present(void) { return !hal_pdetb_get(); }
