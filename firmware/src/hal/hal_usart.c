#include <stddef.h>

#include "gd32e23x.h"
#include "hal_gpio.h"
#include "hal_usart.h"

/* USART0 sits on APB2; rcu_periph_clock_enable(RCU_USART0) toggles that bit. */
#define HAL_USART_BAUDRATE 921600U

/* Fixed GD32E23x DMA request routing: USART0_TX -> channel 2, USART0_RX -> channel 1. */
#define HAL_USART_TX_DMA_CH DMA_CH2
#define HAL_USART_RX_DMA_CH DMA_CH1

/* Transmit backstop. A full AT frame clears the wire in well under a millisecond,
 * so this only trips when the line is mis-wired or the transceiver is held off. */
#define HAL_USART_TX_TIMEOUT_MS 100U

/* SysTick is run as a polled 1 kHz down-counter with its interrupt left off, so
 * the wait loops below can bound themselves without owning the global tick that
 * the timer HAL will later install. */
static void hal_usart_tick_init(void) {
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

/* Advance and return a monotonic millisecond counter. COUNTFLAG pulses once per
 * wrap and self-clears on read, so callers in a tight loop must call this every
 * iteration to avoid missing a wrap. */
static uint32_t hal_usart_tick_ms(void) {
    static uint32_t ms;
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
        ms++;
    }
    return ms;
}

void hal_usart_init(void) {
    /* Enable the USART0 (APB2) and DMA (AHB) clocks. */
    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(RCU_DMA);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, HAL_USART_BAUDRATE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);

    /* Let the USART raise DMA requests on each byte; the channel enable gates the
     * actual transfer. */
    usart_dma_transmit_config(USART0, USART_DENT_ENABLE);
    usart_dma_receive_config(USART0, USART_DENR_ENABLE);

    dma_deinit(HAL_USART_TX_DMA_CH);
    dma_deinit(HAL_USART_RX_DMA_CH);

    usart_enable(USART0);

    /* Default to listening; hal_usart_send flips this for the duration of a frame. */
    hal_tr_switch_set(false);

    hal_usart_tick_init();
}

uint16_t hal_usart_send(const uint8_t *data, uint16_t len) {
    dma_parameter_struct dma;

    if (data == NULL || len == 0U) {
        return 0U;
    }

    hal_tr_switch_set(true);

    dma_struct_para_init(&dma);
    dma.periph_addr = (uint32_t)&USART_TDATA(USART0);
    dma.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma.memory_addr = (uint32_t)data;
    dma.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma.number = len;
    dma.priority = DMA_PRIORITY_MEDIUM;
    dma.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_deinit(HAL_USART_TX_DMA_CH);
    dma_init(HAL_USART_TX_DMA_CH, &dma);

    /* TC asserts only after the final stop bit is on the wire, so waiting on it
     * (rather than the DMA finish flag) keeps T/R_SWITCH high for the whole frame. */
    usart_flag_clear(USART0, USART_FLAG_TC);
    dma_channel_enable(HAL_USART_TX_DMA_CH);

    uint32_t start = hal_usart_tick_ms();
    while (usart_flag_get(USART0, USART_FLAG_TC) == RESET) {
        if ((hal_usart_tick_ms() - start) >= HAL_USART_TX_TIMEOUT_MS) {
            break;
        }
    }

    dma_channel_disable(HAL_USART_TX_DMA_CH);
    hal_tr_switch_set(false);

    return (uint16_t)(len - dma_transfer_number_get(HAL_USART_TX_DMA_CH));
}

uint16_t hal_usart_recv(uint8_t *buf, uint16_t maxlen, uint32_t timeout_ms) {
    dma_parameter_struct dma;

    if (buf == NULL || maxlen == 0U) {
        return 0U;
    }

    /* Drop a stale idle flag so the next one frames this reply exclusively. */
    usart_flag_clear(USART0, USART_FLAG_IDLE);

    dma_struct_para_init(&dma);
    dma.periph_addr = (uint32_t)&USART_RDATA(USART0);
    dma.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma.memory_addr = (uint32_t)buf;
    dma.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma.number = maxlen;
    dma.priority = DMA_PRIORITY_MEDIUM;
    dma.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_deinit(HAL_USART_RX_DMA_CH);
    dma_init(HAL_USART_RX_DMA_CH, &dma);
    dma_channel_enable(HAL_USART_RX_DMA_CH);

    /* A reply ends when the buffer fills or the line falls idle after the last
     * byte. Otherwise the timeout bounds the wait for a non-response. */
    uint32_t start = hal_usart_tick_ms();
    while ((dma_flag_get(HAL_USART_RX_DMA_CH, DMA_FLAG_FTF) == RESET) &&
           (usart_flag_get(USART0, USART_FLAG_IDLE) == RESET)) {
        if ((hal_usart_tick_ms() - start) >= timeout_ms) {
            break;
        }
    }

    dma_channel_disable(HAL_USART_RX_DMA_CH);
    usart_flag_clear(USART0, USART_FLAG_IDLE);

    return (uint16_t)(maxlen - dma_transfer_number_get(HAL_USART_RX_DMA_CH));
}

uint16_t hal_usart_send_recv(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_max,
                             uint32_t timeout_ms) {
    (void)hal_usart_send(tx, tx_len);
    return hal_usart_recv(rx, rx_max, timeout_ms);
}
