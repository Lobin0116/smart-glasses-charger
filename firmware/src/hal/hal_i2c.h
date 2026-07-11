#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>

/* I2C0 single-master bus for the charger's analog companions: the CW2017 fuel
 * gauge (address 0x63) and the IP5353 power-path IC (control 0x74, status 0x75).
 * The bus runs at 200 kHz on PB6 SCL / PB7 SDA, whose AF open-drain pin setup is
 * owned by hal_gpio. Every transfer is a blocking master operation bounded by a
 * 100 ms per-stage timeout against the SysTick counter; it returns 0 on success
 * and -1 on any bus stall, slave NACK, or timeout. */

/* Enable the I2C0 clock, reset the peripheral, and bring it up as a 200 kHz,
 * 7-bit-address master with ACK enabled. Call after hal_gpio_init (pinmux) and
 * hal_timer_init (the timeout source). */
void hal_i2c_init(void);

/* Master write: START, address addr7 for write, push len bytes, STOP. A len of 0
 * sends the address only, which doubles as a device-present probe. */
int hal_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);

/* Write a register/command byte followed by len payload bytes in a single
 * transaction, matching the IP5353 control interface that prefixes writes with a
 * register byte. */
int hal_i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len);

/* Random read: write reg, issue a repeated START, address addr7 for read, then
 * drain len bytes into buf. The final byte is NACKed so the slave releases the bus
 * before STOP. */
int hal_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len);

#endif /* HAL_I2C_H */
