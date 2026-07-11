#ifndef CW2017_H
#define CW2017_H

#include <stdint.h>

/* CW2017 lithium fuel-gauge driver. The gauge sits on I2C0 at 7-bit address
 * 0x63 and tracks a single cell, exposing state of charge, cell voltage, and
 * temperature over a handful of registers. This layer is a thin converter on top
 * of hal_i2c: it runs the power-up sequence and scales the gauge's raw register
 * reads into the engineering units the rest of the firmware consumes. */

/* Battery snapshot reported by cw2017_get_status. soc is the integer state of
 * charge (0-100), voltage_mv the cell terminal voltage, and temp_c the gauge's
 * thermistor reading in degrees Celsius. */
typedef struct {
    uint8_t soc;
    uint16_t voltage_mv;
    int8_t temp_c;
} cw2017_status_t;

/* Wake the gauge, quick-start its SOC engine to clear any sleep state, and
 * confirm the chip answers with its fixed version id. Call once after the I2C
 * bus is up. Returns 0 on a present, running CW2017, -1 if the device does not
 * answer, rejects the power-up writes, or reports an unexpected id. */
int cw2017_init(void);

/* Read the cell state of charge as an integer percentage (0-100). Returns 0 on a
 * bus fault; callers that must distinguish a genuine 0% from a failed transfer
 * should use cw2017_get_status instead. */
uint8_t cw2017_get_soc(void);

/* Read the cell terminal voltage in millivolts. Returns 0 on a bus fault. */
uint16_t cw2017_get_voltage_mv(void);

/* Read the cell temperature in degrees Celsius. Returns 0 on a bus fault. */
int8_t cw2017_get_temp_c(void);

/* Read soc, voltage, and temperature into *status in one call. Returns 0 if
 * every register read succeeded, -1 if any transfer faulted -- in which case
 * *status is left untouched and must not be treated as a trustworthy snapshot. */
int cw2017_get_status(cw2017_status_t *status);

#endif /* CW2017_H */
