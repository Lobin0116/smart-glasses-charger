#ifndef MT5706_H
#define MT5706_H

#include <stdbool.h>

/* MT5706 Qi2.0 wireless charge receiver. The receiver's I2C register map and
 * slave address are not yet confirmed, so this layer owns only the CHIP_EN2
 * power switch (PB11) and latches the COIL_INTB edge (PA12) for the main loop
 * to drain. Register-level accessors slot in alongside these once the address
 * and command set are pinned down. */

/* Assert CHIP_EN2 (high) to power the receiver and enable the coil; drop it to
 * cut power. */
void mt5706_enable(void);
void mt5706_disable(void);

/* Enable state tracked through the pair above. */
bool mt5706_is_enabled(void);

/* Invoked from the EXTI callback when COIL_INTB falls. Sets a pending-event
 * flag the main loop polls; safe to call from interrupt context. */
void mt5706_on_interrupt(void);

bool mt5706_has_event(void);
void mt5706_clear_event(void);

#endif /* MT5706_H */
