#include <stdbool.h>
#include <stddef.h>

#include "gd32e23x.h"
#include "hal_i2c.h"
#include "hal_timer.h"

/* 200 kHz standard mode: well within the 400 kHz both the CW2017 and IP5353
 * tolerate, and slow enough to keep rise-time margins comfortable on the onboard
 * pull-ups. */
#define HAL_I2C_BUS_FREQ_HZ 200000U

/* Per-stage deadline. Each wait arms its own start point, so a long burst does not
 * burn the whole budget on its first flag. */
#define HAL_I2C_TIMEOUT_MS 100U

/* Spin until flag reads the expected level, or the 100 ms budget elapses. */
static bool hal_i2c_wait_flag(i2c_flag_enum flag, FlagStatus expected) {
    uint32_t start = hal_timer_get_ms();
    while (i2c_flag_get(I2C0, flag) != expected) {
        if (hal_timer_expired(start, HAL_I2C_TIMEOUT_MS)) {
            return false;
        }
    }
    return true;
}

/* Spin until flag asserts, bailing early on an acknowledge error (a slave NACK on
 * its address or a data byte) or the 100 ms budget. */
static bool hal_i2c_wait_or_err(i2c_flag_enum flag) {
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

/* Wait for the bus to read idle before asserting START. A slave holding a line low
 * parks I2CBSY; the timeout turns that into a normal failure rather than a hang. */
static bool hal_i2c_wait_idle(void) {
    if (hal_i2c_wait_flag(I2C_FLAG_I2CBSY, RESET)) {
        return true;
    }
    /* Bus stuck: attempt recovery. */
    hal_i2c_bus_recover();
    return hal_i2c_wait_flag(I2C_FLAG_I2CBSY, RESET);
}

void hal_i2c_init(void) {
    rcu_periph_clock_enable(RCU_I2C0);
    i2c_deinit(I2C0);
    i2c_clock_config(I2C0, HAL_I2C_BUS_FREQ_HZ, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0U);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

/* Recover from a bus lock: a slave that held SDA low through a mid-transfer
 * reset will hold the master's START condition hostage. Toggling SCL nine
 * times manually clocks the stuck byte out and lets the slave release SDA,
 * after which a STOP on the GPIO layer frees the bus for a fresh start. */
void hal_i2c_bus_recover(void) {
    /* Switch PB6/PB7 to GPIO output mode for manual clocking. */
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_PIN_6 | GPIO_PIN_7);

    /* Release both lines (open-drain high). */
    gpio_bit_set(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    for (int i = 0; i < 9; i++) {
        if (gpio_input_bit_get(GPIOB, GPIO_PIN_7)) {
            break; /* SDA released, no need for more clocks */
        }
        gpio_bit_reset(GPIOB, GPIO_PIN_6); /* SCL low */
        hal_timer_delay_ms(1);
        gpio_bit_set(GPIOB, GPIO_PIN_6); /* SCL high */
        hal_timer_delay_ms(1);
    }

    /* Generate a manual STOP: SDA rises while SCL is high. */
    gpio_bit_reset(GPIOB, GPIO_PIN_7); /* SDA low */
    hal_timer_delay_ms(1);
    gpio_bit_set(GPIOB, GPIO_PIN_6); /* SCL high */
    hal_timer_delay_ms(1);
    gpio_bit_set(GPIOB, GPIO_PIN_7); /* SDA high = STOP */

    hal_timer_delay_ms(1);

    /* Restore PB6/PB7 to I2C0 alternate function. */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_10MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_af_set(GPIOB, GPIO_AF_1, GPIO_PIN_6 | GPIO_PIN_7);

    i2c_deinit(I2C0);
    i2c_clock_config(I2C0, HAL_I2C_BUS_FREQ_HZ, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0U);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

/* Assert START, send addr7 in the given direction, and confirm the slave answered.
 * A stale AERR from a prior NACK is cleared first so it cannot false-trip the wait.
 * On a NACK the hardware emits STOP on its own, so the caller need not recover. */
static bool hal_i2c_master_addr(uint8_t addr7, uint32_t direction) {
    i2c_flag_clear(I2C0, I2C_FLAG_AERR);
    i2c_start_on_bus(I2C0);
    if (!hal_i2c_wait_flag(I2C_FLAG_SBSEND, SET)) {
        return false;
    }
    i2c_master_addressing(I2C0, addr7, direction);
    if (!hal_i2c_wait_or_err(I2C_FLAG_ADDSEND)) {
        return false;
    }
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    return true;
}

/* Push one byte: wait for the TX register to drain, load it, then wait until it
 * has fully left the shift register (BTC). A NACK on the byte surfaces as AERR. */
static bool hal_i2c_tx_byte(uint8_t byte) {
    if (!hal_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        return false;
    }
    i2c_data_transmit(I2C0, byte);
    return hal_i2c_wait_or_err(I2C_FLAG_BTC);
}

int hal_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len) {
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
    /* Let STOP propagate so the bus reads idle before the next caller. */
    (void)hal_i2c_wait_idle();
    return 0;
}

int hal_i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len) {
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

int hal_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len) {
    if (len == 0U || buf == NULL) {
        return -1;
    }

    /* Phase 1: point the slave at reg without releasing the bus. */
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

    /* Phase 2: repeated START into a read. master_addr asserts START with no
     * intervening STOP, so this is a genuine repeated start, not a new session. */
    if (!hal_i2c_master_addr(addr7, I2C_RECEIVER)) {
        return -1;
    }

    /* NACK the final byte by clearing ACKEN before its 9th clock. On the last pass
     * that lands right after the address phase (len == 1) or the instant the prior
     * byte was drained (len > 1) -- a full byte time of margin at 200 kHz. STOP is
     * queued alongside it so the bus is released as soon as the byte lands. */
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
