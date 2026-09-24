#include <stddef.h>

#include "cw2017.h"

#include "hal_i2c.h"
#include "hal_timer.h"

#define CW2017_I2C_ADDR 0x63U

#define CW2017_REG_VCELL_H 0x02U
#define CW2017_REG_SOC_H 0x04U
#define CW2017_REG_TEMP 0x06U
#define CW2017_REG_CONFIG 0x08U
#define CW2017_REG_SOC_ALERT 0x0BU
#define CW2017_REG_BATINFO 0x10U

#define CW2017_CONFIG_QUICKSTART 0x30U
#define CW2017_CONFIG_NORMAL 0x00U

#define CW2017_CONFIG_SLEEP 0xC0U

#define CW2017_SOC_ALERT_UPDATE_FLAG 0x80U
#define CW2017_PROFILE_SIZE 80U
static const uint8_t cw2017_profile[CW2017_PROFILE_SIZE] = {
    0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9E, 0xC8, 0xD2, 0xC5, 0xC2, 0xCF, 0x53, 0x25,
    0x10, 0xF5, 0xEB, 0xE1, 0xB7, 0x93, 0x83, 0x6E,
    0x5D, 0x4D, 0x42, 0x54, 0x94, 0xDC, 0x76, 0xD7,
    0xD7, 0xD2, 0xD2, 0xD0, 0xCE, 0xCC, 0xC4, 0xCD,
    0xC3, 0xBD, 0xCB, 0xAE, 0x96, 0x8A, 0x83, 0x75,
    0x67, 0x61, 0x76, 0x8C, 0xA4, 0x96, 0x50, 0x66,
    0x00, 0x00, 0x90, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22,
};

static int cw2017_read_soc(uint8_t *soc) { return hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_SOC_H, soc, 1U); }

static int cw2017_read_voltage_mv(uint16_t *mv)
{
    uint8_t buf[2];
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_VCELL_H, buf, sizeof(buf)) != 0) {
        return -1;
    }
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    raw &= 0x3FFFU;
    *mv = (uint16_t)((raw * 5U) / 16U);
    return 0;
}

static int cw2017_read_temp_c(int8_t *temp)
{
    uint8_t raw;
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_TEMP, &raw, 1U) != 0) {
        return -1;
    }
    *temp = (int8_t)(-40 + raw / 2);
    return 0;
}

static int cw2017_burn_profile(void)
{
    for (uint8_t i = 0U; i < CW2017_PROFILE_SIZE; i++) {
        if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_BATINFO + i, &cw2017_profile[i], 1U) != 0) {
            return -1;
        }
    }
    uint8_t alert;
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_SOC_ALERT, &alert, 1U) != 0) {
        return -1;
    }
    alert |= CW2017_SOC_ALERT_UPDATE_FLAG;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_SOC_ALERT, &alert, 1U) != 0) {
        return -1;
    }

    uint8_t mode = CW2017_CONFIG_NORMAL;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &mode, 1U) != 0) {
        return -1;
    }
    return 0;
}

static bool cw2017_verify_profile(void)
{
    for (uint8_t i = 0U; i < CW2017_PROFILE_SIZE; i++) {
        uint8_t val;
        if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_BATINFO + i, &val, 1U) != 0) {
            return false;
        }
        if (val != cw2017_profile[i]) {
            return false;
        }
    }
    return true;
}

int cw2017_init(void)
{

    uint8_t mode = 0U;
    uint8_t alert = 0U;
    bool need_burn = true;
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &mode, 1U) == 0
        && hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_SOC_ALERT, &alert, 1U) == 0) {
        if (mode == CW2017_CONFIG_NORMAL && (alert & CW2017_SOC_ALERT_UPDATE_FLAG) != 0U
            && cw2017_verify_profile()) {
            need_burn = false;
        }
    }
    if (need_burn) {
        if (cw2017_burn_profile() != 0) {
            return -1;
        }
    }

    uint8_t cfg = CW2017_CONFIG_QUICKSTART;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U) != 0) {
        return -1;
    }
    cfg = CW2017_CONFIG_NORMAL;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U) != 0) {
        return -1;
    }
    return 0;
}

uint8_t cw2017_get_soc(void)
{
    uint8_t soc = 0U;
    if (cw2017_read_soc(&soc) != 0) {

        return 100U;
    }
    if (soc == 0U || soc > 100U) {

        return 100U;
    }
    return soc;
}

uint16_t cw2017_get_voltage_mv(void)
{
    uint16_t mv = 0U;
    (void)cw2017_read_voltage_mv(&mv);
    return mv;
}

int8_t cw2017_get_temp_c(void)
{
    int8_t temp = 0;
    (void)cw2017_read_temp_c(&temp);
    return temp;
}

void cw2017_enter_sleep(void)
{

    uint8_t cfg = CW2017_CONFIG_SLEEP;
    (void)hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U);
}

void cw2017_resume(void)
{

    uint8_t cfg = CW2017_CONFIG_QUICKSTART;
    (void)hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U);
    cfg = CW2017_CONFIG_NORMAL;
    (void)hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &cfg, 1U);
}

int cw2017_get_status(cw2017_status_t *status)
{
    if (status == NULL) {
        return -1;
    }

    uint8_t soc;
    uint16_t mv;
    int8_t temp;
    if (cw2017_read_soc(&soc) != 0 || cw2017_read_voltage_mv(&mv) != 0 || cw2017_read_temp_c(&temp) != 0) {
        return -1;
    }

    status->soc = soc;
    status->voltage_mv = mv;
    status->temp_c = temp;
    return 0;
}
