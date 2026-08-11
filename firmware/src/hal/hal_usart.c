#include <stdbool.h>
#include <stddef.h>

#include "gd32e23x.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "hal_usart.h"

#define HAL_USART_BAUDRATE    115200U
#define HAL_USART_RX_DMA_CH   DMA_CH2
#define HAL_USART_RX_BUF_SIZE 256U

static volatile uint8_t rx_buf[HAL_USART_RX_BUF_SIZE];
static uint16_t rx_tail;

void hal_usart_init(void)
{
    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(RCU_DMA);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, HAL_USART_BAUDRATE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);

    usart_dma_receive_config(USART0, USART_DENR_ENABLE);

    dma_parameter_struct dma;
    dma_struct_para_init(&dma);
    dma.periph_addr = (uint32_t)&USART_RDATA(USART0);
    dma.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma.memory_addr = (uint32_t)rx_buf;
    dma.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma.number = HAL_USART_RX_BUF_SIZE;
    dma.priority = DMA_PRIORITY_MEDIUM;
    dma.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_deinit(HAL_USART_RX_DMA_CH);
    dma_init(HAL_USART_RX_DMA_CH, &dma);
    dma_circulation_enable(HAL_USART_RX_DMA_CH);
    dma_channel_enable(HAL_USART_RX_DMA_CH);

    rx_tail = 0U;

    usart_enable(USART0);

    /* Flush any bytes the DMA may have picked up between dma_channel_enable
     * and usart_enable (e.g. noise during rail settling) so the first
     * hal_usart_rx_get caller does not see garbage. */
    hal_usart_rx_clear();

    hal_tr_switch_set(false);
}

static uint16_t rx_head_get(void)
{
    return (uint16_t)(HAL_USART_RX_BUF_SIZE - dma_transfer_number_get(HAL_USART_RX_DMA_CH));
}

void hal_usart_rx_clear(void)
{
    rx_tail = rx_head_get();
}

bool hal_usart_rx_get(uint8_t *c)
{
    uint16_t head = rx_head_get();
    if (head == rx_tail) {
        return false;
    }
    *c = rx_buf[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % HAL_USART_RX_BUF_SIZE);
    return true;
}

bool hal_usart_rx_peek(uint8_t *c)
{
    uint16_t head = rx_head_get();
    if (head == rx_tail) {
        return false;
    }
    *c = rx_buf[rx_tail];
    return true;
}

bool hal_usart_rx_peek_n(uint8_t *buf, uint16_t n)
{
    if (buf == NULL || n == 0U) {
        return n == 0U;
    }
    uint16_t head = rx_head_get();
    uint16_t available;
    if (head >= rx_tail) {
        available = (uint16_t)(head - rx_tail);
    } else {
        available = (uint16_t)(HAL_USART_RX_BUF_SIZE - rx_tail + head);
    }
    if (available < n) {
        return false;
    }
    for (uint16_t i = 0U; i < n; i++) {
        buf[i] = rx_buf[(uint16_t)((rx_tail + i) % HAL_USART_RX_BUF_SIZE)];
    }
    return true;
}

uint16_t hal_usart_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U) {
        return 0U;
    }

    hal_tr_switch_set(true);

    for (uint16_t i = 0U; i < len; i++) {
        while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET) {
        }
        usart_data_transmit(USART0, data[i]);
    }
    while (usart_flag_get(USART0, USART_FLAG_TC) == RESET) {
    }

    hal_tr_switch_set(false);

    return len;
}

uint16_t hal_usart_recv(uint8_t *buf, uint16_t maxlen, uint32_t timeout_ms)
{
    if (buf == NULL || maxlen == 0U) {
        return 0U;
    }

    uint16_t n = 0U;
    uint32_t start = hal_timer_get_ms();
    while (n < maxlen) {
        if (hal_usart_rx_get(&buf[n])) {
            n++;
            start = hal_timer_get_ms();
        }
        if (hal_timer_expired(start, timeout_ms)) {
            break;
        }
    }
    return n;
}

uint16_t hal_usart_send_recv(const uint8_t *tx, uint16_t tx_len, uint8_t *rx, uint16_t rx_max, uint32_t timeout_ms)
{
    (void)hal_usart_send(tx, tx_len);
    return hal_usart_recv(rx, rx_max, timeout_ms);
}
