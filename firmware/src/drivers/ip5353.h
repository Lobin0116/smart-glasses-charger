#ifndef IP5353_H
#define IP5353_H

#include <stdbool.h>
#include <stdint.h>
#define IP5353_ADDR_CONTROL 0x74U
#define IP5353_ADDR_STATUS 0x75U

#define IP5353_REG_SYS_STATE0 0x45U
#define IP5353_REG_SYS_STATE2 0x50U
#define IP5353_REG_SYS_STATE5 0x69U
#define IP5353_REG_NTC_STATE 0x6FU

#define IP5353_CHG_STATE_NOT_CHARGING 0x00U
#define IP5353_CHG_STATE_CC 0x02U
#define IP5353_CHG_STATE_CV 0x03U
#define IP5353_CHG_STATE_FULL 0x05U

typedef struct
{
    uint8_t reserved0 : 2;
    uint8_t vinov : 1;
    uint8_t vbusok : 1;
    uint8_t reserved4 : 1;
    uint8_t vinok : 1;
    uint8_t vbusov : 1;
    uint8_t reserved7 : 1;
} ip5353_sys_state0_t;

typedef struct
{
    uint8_t sys_state : 3;
    uint8_t reserved3 : 1;
    uint8_t boost_en : 1;
    uint8_t charge_en : 1;
    uint8_t reserved6 : 2;
} ip5353_sys_state2_t;

int ip5353_read_sys_state0(ip5353_sys_state0_t *state);

int ip5353_read_sys_state2(ip5353_sys_state2_t *state);

int ip5353_read_chg_state(uint8_t *chg_state);

int ip5353_read_modify_write(uint8_t addr7, uint8_t reg, uint8_t mask, uint8_t val);

bool ip5353_is_charging(void);

bool ip5353_is_input_valid(void);

bool ip5353_is_full(void);

#endif
