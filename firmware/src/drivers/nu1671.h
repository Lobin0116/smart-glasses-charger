#ifndef NU1671_H
#define NU1671_H

#include <stdbool.h>
#define NU1671_I2C_ADDR 0x34U

void nu1671_on_interrupt(void);

bool nu1671_poll(void);

void nu1671_probe(void);
bool nu1671_is_present(void);

void nu1671_pdet_enable(bool enable);
bool nu1671_tx_pad_present(void);

#endif
