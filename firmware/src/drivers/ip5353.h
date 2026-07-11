#ifndef IP5353_H
#define IP5353_H

#include <stdbool.h>
#include <stdint.h>

/* IP5353 power-path SOC driver. The IC exposes its registers across two I2C
 * addresses on the same bus: the control register file at 0x74 and the
 * read-only status register file at 0x75. Charge/boost behaviour is governed by
 * the control file; everything observed here (input rails, charge progress) is
 * read from the status file.
 *
 * The IP5353 gates its I2C interface behind the INT pin (PA11): registers are
 * only valid once INT has rested high for 100ms. Every access in this driver
 * arms that settle window automatically, so callers never have to time it.
 * Requires hal_i2c_init, hal_timer_init, and hal_gpio_init (which owns the INT
 * input) to have run first. */

/* 7-bit I2C addresses. The 8-bit forms are 0xE8/0xE9 (control) and 0xEA/0xEB
 * (status); hal_i2c takes the 7-bit form and shifts it itself. */
#define IP5353_ADDR_CONTROL 0x74U
#define IP5353_ADDR_STATUS 0x75U

/* Status register offsets (file 0x75). */
#define IP5353_REG_SYS_STATE0 0x45U /* VIN/VBUS validity and over-voltage flags */
#define IP5353_REG_SYS_STATE2 0x50U /* charge/boost enables and top-level sys_state */
#define IP5353_REG_SYS_STATE5 0x69U /* battery charge progression state */
#define IP5353_REG_NTC_STATE 0x6FU  /* NTC temperature protection flags */

/* Decoded charge state from SYS_STATE5 bits[6:4]. */
#define IP5353_CHG_STATE_NOT_CHARGING 0x00U
#define IP5353_CHG_STATE_CC 0x02U   /* constant-current charging */
#define IP5353_CHG_STATE_CV 0x03U   /* constant-voltage charging */
#define IP5353_CHG_STATE_FULL 0x05U /* charge terminated */

/* SYS_STATE0 (0x45): input rail status. */
typedef struct {
    uint8_t reserved0 : 2; /* bits[1:0] */
    uint8_t vinov : 1;     /* bit2: VIN over-voltage detected */
    uint8_t vbusok : 1;    /* bit3: VBUS input valid */
    uint8_t reserved4 : 1; /* bit4 */
    uint8_t vinok : 1;     /* bit5: VIN input valid */
    uint8_t vbusov : 1;    /* bit6: VBUS over-voltage detected */
    uint8_t reserved7 : 1; /* bit7 */
} ip5353_sys_state0_t;

/* SYS_STATE2 (0x50): power-path enables and coarse system state. */
typedef struct {
    uint8_t sys_state : 3; /* bits[2:0]: 000 idle, 101 charging, 010 boost active */
    uint8_t reserved3 : 1; /* bit3 */
    uint8_t boost_en : 1;  /* bit4: boost converter enabled */
    uint8_t charge_en : 1; /* bit5: charger enabled */
    uint8_t reserved6 : 2; /* bits[7:6] */
} ip5353_sys_state2_t;

/* Read SYS_STATE0 (0x45) from the status file into decoded bit fields. Returns
 * 0 on success, -1 if INT is not settled or the bus transfer fails. */
int ip5353_read_sys_state0(ip5353_sys_state0_t *state);

/* Read SYS_STATE2 (0x50) from the status file into decoded bit fields. Returns
 * 0 on success, -1 if INT is not settled or the bus transfer fails. */
int ip5353_read_sys_state2(ip5353_sys_state2_t *state);

/* Read SYS_STATE5 (0x69) and return bits[6:4] as a IP5353_CHG_STATE_* constant.
 * Returns 0 on success, -1 if INT is not settled or the bus transfer fails. */
int ip5353_read_chg_state(uint8_t *chg_state);

/* Atomic read-modify-write of a single register: clear the bits covered by
 * mask, then set them from val (also masked), leaving every other bit intact.
 * The IP5353 forbids blind writes, so this is the only sanctioned way to touch
 * a control register. addr7 selects 0x74 (control) or 0x75 (status). Returns 0
 * on success, -1 if INT is not settled or any transfer fails. */
int ip5353_read_modify_write(uint8_t addr7, uint8_t reg, uint8_t mask, uint8_t val);

/* True while the battery is in the CC or CV phase of charging. */
bool ip5353_is_charging(void);

/* True if either input rail (VIN or VBUS) is valid. */
bool ip5353_is_input_valid(void);

/* True once the charger reports the charge-terminated (full) state. */
bool ip5353_is_full(void);

#endif /* IP5353_H */
