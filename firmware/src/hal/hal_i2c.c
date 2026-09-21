#include <stdbool.h>
#include <stddef.h>

#include "gd32e23x.h"
#include "hal_i2c.h"
#include "hal_timer.h"

#define HAL_I2C_BUS_FREQ_HZ 200000U

#define HAL_I2C_TIMEOUT_MS 100U

static bool hal_i2c_wait_flag(i2c_flag_enum flag, FlagStatus expected)
{
    uint32_t start = hal_timer_get_ms();
    while (i2c_flag_get(I2C0, flag) != expected) {
        if (hal_timer_expired(start, HAL_I2C_TIMEOUT_MS)) {
            return false;
        }
    }
    return true;
}

static bool hal_i2c_wait_or_err(i2c_flag_enum flag)
{
    uint32_t start = hal_timer_get_ms();
    while (i2c_flag_get(I2C0, flag) == RESET) {
        if (i2c_flag_get(I2C0, I2C_FLAG_AERR) == SET) {
            i2c_flag_clear(I2C0, I2C_FLAG_AERR);
            return false;
        }
        if (hal_timer_expired(start, HAL_I2C_TIMEOUT_MS)) {
            return false;
        }
    }
    return true;
}

static bool hal_i2c_wait_idle(void)
{
    if (hal_i2c_wait_flag(I2C_FLAG_I2CBSY, RESET)) {
        return true;
    }

    hal_i2c_bus_recover();
    return hal_i2c_wait_flag(I2C_FLAG_I2CBSY, RESET);
}

void hal_i2c_init(void)
{
    rcu_periph_clock_enable(RCU_I2C0);
    i2c_deinit(I2C0);
    i2c_clock_config(I2C0, HAL_I2C_BUS_FREQ_HZ, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0U);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

void hal_i2c_bus_recover(void)
{

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_6 | GPIO_PIN_7);

    gpio_bit_set(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    for (int i = 0; i < 9; i++) {
        if (gpio_input_bit_get(GPIOB, GPIO_PIN_7)) {
            break;
        }
        gpio_bit_reset(GPIOB, GPIO_PIN_6);
        hal_timer_delay_ms(1);
        gpio_bit_set(GPIOB, GPIO_PIN_6);
        hal_timer_delay_ms(1);
    }

    gpio_bit_reset(GPIOB, GPIO_PIN_7);
    hal_timer_delay_ms(1);
    gpio_bit_set(GPIOB, GPIO_PIN_6);
    hal_timer_delay_ms(1);
    gpio_bit_set(GPIOB, GPIO_PIN_7);

    hal_timer_delay_ms(1);

    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_10MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_af_set(GPIOB, GPIO_AF_1, GPIO_PIN_6 | GPIO_PIN_7);

    i2c_deinit(I2C0);
    i2c_clock_config(I2C0, HAL_I2C_BUS_FREQ_HZ, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0U);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

static bool hal_i2c_master_addr(uint8_t addr7, uint32_t direction)
{
    i2c_flag_clear(I2C0, I2C_FLAG_AERR);
    i2c_start_on_bus(I2C0);
    if (!hal_i2c_wait_flag(I2C_FLAG_SBSEND, SET)) {
        i2c_stop_on_bus(I2C0);
        return false;
    }
    i2c_master_addressing(I2C0, (uint32_t)addr7 << 1U, direction);
    if (!hal_i2c_wait_or_err(I2C_FLAG_ADDSEND)) {
        i2c_stop_on_bus(I2C0);
        return false;
    }
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    return true;
}

static bool hal_i2c_tx_byte(uint8_t byte)
{
    if (!hal_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        return false;
    }
    i2c_data_transmit(I2C0, byte);
    return hal_i2c_wait_or_err(I2C_FLAG_BTC);
}

int hal_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    if (len != 0U && data == NULL) {
        return -1;
    }
    if (!hal_i2c_wait_idle()) {
        return -1;
    }
    if (!hal_i2c_master_addr(addr7, I2C_TRANSMITTER)) {
        return -1;
    }
    for (uint16_t i = 0U; i < len; i++) {
        if (!hal_i2c_tx_byte(data[i])) {
            i2c_stop_on_bus(I2C0);
            return -1;
        }
    }
    i2c_stop_on_bus(I2C0);

    (void)hal_i2c_wait_idle();
    return 0;
}

int hal_i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (len != 0U && data == NULL) {
        return -1;
    }
    if (!hal_i2c_wait_idle()) {
        return -1;
    }
    if (!hal_i2c_master_addr(addr7, I2C_TRANSMITTER)) {
        return -1;
    }
    if (!hal_i2c_tx_byte(reg)) {
        i2c_stop_on_bus(I2C0);
        return -1;
    }
    for (uint16_t i = 0U; i < len; i++) {
        if (!hal_i2c_tx_byte(data[i])) {
            i2c_stop_on_bus(I2C0);
            return -1;
        }
    }
    i2c_stop_on_bus(I2C0);
    (void)hal_i2c_wait_idle();
    return 0;
}

int hal_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (len == 0U || buf == NULL) {
        return -1;
    }

    if (!hal_i2c_wait_idle()) {
        return -1;
    }
    if (!hal_i2c_master_addr(addr7, I2C_TRANSMITTER)) {
        return -1;
    }
    if (!hal_i2c_tx_byte(reg)) {
        i2c_stop_on_bus(I2C0);
        return -1;
    }

    if (!hal_i2c_master_addr(addr7, I2C_RECEIVER)) {
        return -1;
    }

    for (uint16_t i = 0U; i < len; i++) {
        if (i == (uint16_t)(len - 1U)) {
            i2c_ack_config(I2C0, I2C_ACK_DISABLE);
            i2c_stop_on_bus(I2C0);
        }
        if (!hal_i2c_wait_flag(I2C_FLAG_RBNE, SET)) {
            i2c_ack_config(I2C0, I2C_ACK_ENABLE);
            return -1;
        }
        buf[i] = i2c_data_receive(I2C0);
    }

    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
    return 0;
}

void hal_i2c_pins_sleep(void)
{

    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
}

void hal_i2c_pins_resume(void)
{

    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_10MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_af_set(GPIOB, GPIO_AF_1, GPIO_PIN_6 | GPIO_PIN_7);
}
