#include <stddef.h>

#include "cw2017.h"

#include "hal_i2c.h"

/* 7-bit I2C address, latched on the bus by hal_i2c. */
#define CW2017_I2C_ADDR 0x63U

/* Register map (see CONTEXT.md). */
#define CW2017_REG_VERSION 0x00U
#define CW2017_REG_VCELL_H 0x02U
#define CW2017_REG_SOC_H 0x04U
#define CW2017_REG_TEMP 0x06U
#define CW2017_REG_CONFIG 0x08U

/* Fixed id read back from VERSION once the gauge is awake and answering. */
#define CW2017_VERSION_ID 0xA0U

/* CONFIG power-up sequence: 0x30 kicks the gauge out of sleep and starts a
 * quick-start of the SOC engine, then 0x00 clears the trigger so it settles back
 * into normal measurement. */
#define CW2017_CONFIG_QUICKSTART 0x30U
#define CW2017_CONFIG_CLEAR 0x00U

/* SOC register: the high byte at 0x04 is the integer percentage; the low byte's
 * 1/256% fraction is dropped, matching the rest of the firmware's 1% grading. */
static int cw2017_read_soc(uint8_t *soc) {
    return hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_SOC_H, soc, 1U);
}

/* VCELL spans 0x02-0x03 as a 14-bit field. The slave auto-increments its
 * register pointer across the two-byte read, so a single transfer from 0x02
 * yields [high, low]. The LSB is 312.5 uV, which is exactly 5/16 mV. */
static int cw2017_read_voltage_mv(uint16_t *mv) {
    uint8_t buf[2];
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_VCELL_H, buf, sizeof(buf)) != 0) {
        return -1;
    }
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    raw &= 0x3FFFU;
    *mv = (uint16_t)((raw * 5U) / 16U);
    return 0;
}

/* TEMP is one byte at 0x06 with 0.5C resolution and a -40C offset. */
static int cw2017_read_temp_c(int8_t *temp) {
    uint8_t raw;
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_TEMP, &raw, 1U) != 0) {
        return -1;
    }
    *temp = (int8_t)(-40 + raw / 2);
    return 0;
}

int cw2017_init(void) {
    uint8_t cfg = CW2017_CONFIG_QUICKSTART;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U) != 0) {
        return -1;
    }
    cfg = CW2017_CONFIG_CLEAR;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U) != 0) {
        return -1;
    }

    uint8_t version;
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_VERSION, &version, 1U) != 0) {
        return -1;
    }
    if (version != CW2017_VERSION_ID) {
        return -1;
    }
    return 0;
}

uint8_t cw2017_get_soc(void) {
    uint8_t soc = 0U;
    (void)cw2017_read_soc(&soc);
    return soc;
}

uint16_t cw2017_get_voltage_mv(void) {
    uint16_t mv = 0U;
    (void)cw2017_read_voltage_mv(&mv);
    return mv;
}

int8_t cw2017_get_temp_c(void) {
    int8_t temp = 0;
    (void)cw2017_read_temp_c(&temp);
    return temp;
}

int cw2017_get_status(cw2017_status_t *status) {
    if (status == NULL) {
        return -1;
    }

    uint8_t soc;
    uint16_t mv;
    int8_t temp;
    if (cw2017_read_soc(&soc) != 0 || cw2017_read_voltage_mv(&mv) != 0 ||
        cw2017_read_temp_c(&temp) != 0) {
        return -1;
    }

    status->soc = soc;
    status->voltage_mv = mv;
    status->temp_c = temp;
    return 0;
}
