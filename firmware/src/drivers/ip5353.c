#include "ip5353.h"

#include <stddef.h>

#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_timer.h"

/* INT must rest high this long before the IP5353 honours I2C, per the operating
 * constraint in CONTEXT.md. */
#define IP5353_INT_SETTLE_MS 100U

/* Tick at which INT was first observed high in the current high period. Reset to
 * 0 whenever INT reads low, so the full settle window re-runs after a drop. The
 * separate int_armed flag keeps tick 0 from being mistaken for "not yet seen". */
static bool int_armed;
static uint32_t int_high_since;

/* Block until INT has been high for the full settle window, or report the chip
 * as unreachable when INT is low. Within one sustained high period the delay
 * shrinks to the remaining time, so a burst of reads only pays the 100ms once. */
static bool ip5353_ensure_ready(void) {
    if (!hal_charger_int_get()) {
        int_armed = false;
        return false;
    }
    if (!int_armed) {
        int_armed = true;
        int_high_since = hal_timer_get_ms();
    }
    uint32_t elapsed = hal_timer_elapsed(int_high_since);
    if (elapsed < IP5353_INT_SETTLE_MS) {
        hal_timer_delay_ms(IP5353_INT_SETTLE_MS - elapsed);
    }
    return true;
}

/* Read one register byte, gated by the INT settle window. */
static int ip5353_read_byte(uint8_t addr7, uint8_t reg, uint8_t *out) {
    if (!ip5353_ensure_ready()) {
        return -1;
    }
    return hal_i2c_read_reg(addr7, reg, out, 1U);
}

/* Write one register byte, gated by the INT settle window. */
static int ip5353_write_byte(uint8_t addr7, uint8_t reg, uint8_t val) {
    if (!ip5353_ensure_ready()) {
        return -1;
    }
    return hal_i2c_write_reg(addr7, reg, &val, 1U);
}

int ip5353_read_sys_state0(ip5353_sys_state0_t *state) {
    if (state == NULL) {
        return -1;
    }
    uint8_t raw;
    if (ip5353_read_byte(IP5353_ADDR_STATUS, IP5353_REG_SYS_STATE0, &raw) != 0) {
        return -1;
    }
    /* Overlay the raw byte onto the bitfield struct. arm-none-eabi-gcc packs
     * bit fields LSB-first, matching the IP5353's little-endian register map. */
    union {
        uint8_t raw;
        ip5353_sys_state0_t bits;
    } conv;
    conv.raw = raw;
    *state = conv.bits;
    return 0;
}

int ip5353_read_sys_state2(ip5353_sys_state2_t *state) {
    if (state == NULL) {
        return -1;
    }
    uint8_t raw;
    if (ip5353_read_byte(IP5353_ADDR_STATUS, IP5353_REG_SYS_STATE2, &raw) != 0) {
        return -1;
    }
    union {
        uint8_t raw;
        ip5353_sys_state2_t bits;
    } conv;
    conv.raw = raw;
    *state = conv.bits;
    return 0;
}

int ip5353_read_chg_state(uint8_t *chg_state) {
    if (chg_state == NULL) {
        return -1;
    }
    uint8_t raw;
    if (ip5353_read_byte(IP5353_ADDR_STATUS, IP5353_REG_SYS_STATE5, &raw) != 0) {
        return -1;
    }
    *chg_state = (uint8_t)((raw >> 4) & 0x07U);
    return 0;
}

int ip5353_read_modify_write(uint8_t addr7, uint8_t reg, uint8_t mask, uint8_t val) {
    uint8_t raw;
    if (ip5353_read_byte(addr7, reg, &raw) != 0) {
        return -1;
    }
    uint8_t updated = (uint8_t)((raw & (uint8_t)~mask) | (val & mask));
    return ip5353_write_byte(addr7, reg, updated);
}

bool ip5353_is_charging(void) {
    uint8_t chg_state;
    if (ip5353_read_chg_state(&chg_state) != 0) {
        return false;
    }
    return chg_state == IP5353_CHG_STATE_CC || chg_state == IP5353_CHG_STATE_CV;
}

bool ip5353_is_input_valid(void) {
    ip5353_sys_state0_t state;
    if (ip5353_read_sys_state0(&state) != 0) {
        return false;
    }
    return state.vinok || state.vbusok;
}

bool ip5353_is_full(void) {
    uint8_t chg_state;
    if (ip5353_read_chg_state(&chg_state) != 0) {
        return false;
    }
    return chg_state == IP5353_CHG_STATE_FULL;
}
