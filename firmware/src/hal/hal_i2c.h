#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
void hal_i2c_init(void);

int hal_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);

int hal_i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len);

int hal_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len);

void hal_i2c_bus_recover(void);

void hal_i2c_pins_sleep(void);
void hal_i2c_pins_resume(void);

#endif
