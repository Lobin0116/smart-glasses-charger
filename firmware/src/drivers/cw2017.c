#include <stddef.h>

#include "cw2017.h"

#include "hal_i2c.h"
#include "hal_timer.h"

/* 7-bit I2C address, latched on the bus by hal_i2c. */
#define CW2017_I2C_ADDR 0x63U

/* Register map (see CONTEXT.md + Cellwise CW2017 Driver V1.4.1). */
#define CW2017_REG_VCELL_H   0x02U
#define CW2017_REG_SOC_H     0x04U
#define CW2017_REG_TEMP      0x06U
#define CW2017_REG_CONFIG    0x08U  /* MODE_CONFIG */
#define CW2017_REG_SOC_ALERT 0x0BU
#define CW2017_REG_BATINFO   0x10U  /* 80-byte battery profile starts here */

/* CONFIG power-up sequence: 0x30 kicks the gauge out of sleep and starts a
 * quick-start of the SOC engine, then 0x00 clears the trigger so it settles back
 * into normal measurement. */
#define CW2017_CONFIG_QUICKSTART 0x30U
#define CW2017_CONFIG_NORMAL     0x00U

/* SOC_ALERT bit7: set by host after writing a new battery profile so the gauge
 * re-evaluates SOC with the updated config (Cellwise demo: CONFIG_UPDATE_FLG). */
#define CW2017_SOC_ALERT_UPDATE_FLAG 0x80U

/* Battery profile for 4.2V Li-ion 2000mAh (source: Cellwise CW2017 Driver V1.4.1
 * demo, matches memory cw2017-battery-profile). Used by cw2017_init to auto-burn
 * on first boot / re-burn if the chip lost its config. */
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

/* SOC register: the high byte at 0x04 is the integer percentage; the low byte's
 * 1/256% fraction is dropped, matching the rest of the firmware's 1% grading. */
static int cw2017_read_soc(uint8_t *soc) { return hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_SOC_H, soc, 1U); }

/* VCELL spans 0x02-0x03 as a 14-bit field. The slave auto-increments its
 * register pointer across the two-byte read, so a single transfer from 0x02
 * yields [high, low]. The LSB is 312.5 uV, which is exactly 5/16 mV. */
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

/* TEMP is one byte at 0x06 with 0.5C resolution and a -40C offset. */
static int cw2017_read_temp_c(int8_t *temp)
{
    uint8_t raw;
    if (hal_i2c_read_reg(CW2017_I2C_ADDR, CW2017_REG_TEMP, &raw, 1U) != 0) {
        return -1;
    }
    *temp = (int8_t)(-40 + raw / 2);
    return 0;
}

/* Write the 80-byte battery profile (0x10..0x5F) and set the UPDATE_FLAG in
 * SOC_ALERT so the gauge reloads it. Returns 0 on success. Layout and sequence
 * from Cellwise demo `cw_update_config_info()`. */
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
    /* Re-arm the gauge: NORMAL mode lets it re-evaluate SOC against the new
     * profile. */
    uint8_t mode = CW2017_CONFIG_NORMAL;
    if (hal_i2c_write_reg(CW2017_I2C_ADDR, CW2017_REG_CONFIG, &mode, 1U) != 0) {
        return -1;
    }
    return 0;
}

/* Read back the 80-byte profile and compare. Returns true if it matches the
 * stored profile word-for-word. Used to skip re-burning on every boot. */
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
    /* Auto-burn check (Cellwise demo `cw_init`):
     *  - First boot or after profile loss: MODE != NORMAL or UPDATE_FLAG clear.
     *  - Otherwise read back the profile and re-burn if it drifted.
     * Skipping this when the chip already has a good profile costs ~80 I2C
     * reads (~10 ms); burning costs ~250 ms but only happens once per chip. */
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

    /* Quick-start the SOC engine so it picks up the (possibly new) profile. */
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
        /* I2C read failed — battery gauge not responding. Assume full so the
         * LED falls into the white band rather than stuck on red low-batt
         * blink. Real fix is a battery profile burned into the CW2017. */
        return 100U;
    }
    if (soc == 0U || soc > 100U) {
        /* Abnormal reading (0 = quickstart transitional or dead battery;
         * >100 = unprofiled gauge). Same fallback as above. */
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
