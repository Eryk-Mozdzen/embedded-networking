#include <stm32f4xx_hal.h>

#include <lwip/apps/lwiperf.h>
#include <lwip/init.h>
#include <lwip/timeouts.h>
#include <netif/ppp/ppp.h>
#include <netif/ppp/pppos.h>

#include "fifo.h"
#include "timer.h"

typedef enum {
    STATE_DISCONNECTED,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_LOOP,
} state_t;

extern UART_HandleTypeDef huart2;

static fifo_t fifo_tx;
static fifo_t fifo_rx;
static timer_t timeout_tx;
static timer_t timeout_rx;
static state_t state;

static volatile uint8_t dma_tx_ready = 1;
static volatile uint8_t dma_tx_buffer[8192];
static volatile uint32_t dma_rx_pos = 0;
static volatile uint8_t dma_rx_buffer[8192];

void SystemClock_Config();
void MX_GPIO_Init();
void MX_DMA_Init();
void MX_USART2_UART_Init();

static void data_send() {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if(dma_tx_ready) {
        const uint32_t len = fifo_read(&fifo_tx, (uint8_t *)dma_tx_buffer, sizeof(dma_tx_buffer));
        if(len > 0) {
            dma_tx_ready = 0;
            HAL_UART_Transmit_DMA(&huart2, (uint8_t *)dma_tx_buffer, len);
        }
    }

    __set_PRIMASK(primask);
}

static void data_recv() {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t pos = sizeof(dma_rx_buffer) - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

    if(pos != dma_rx_pos) {
        if(pos > dma_rx_pos) {
            fifo_write(&fifo_rx, (uint8_t *)&dma_rx_buffer[dma_rx_pos], pos - dma_rx_pos);
        } else {
            fifo_write(&fifo_rx, (uint8_t *)&dma_rx_buffer[dma_rx_pos],
                       sizeof(dma_rx_buffer) - dma_rx_pos);
            if(pos > 0) {
                fifo_write(&fifo_rx, (uint8_t *)dma_rx_buffer, pos);
            }
        }
        dma_rx_pos = pos;
    }

    __set_PRIMASK(primask);
}

static void pppos_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;

    if(err_code == PPPERR_NONE) {
        if(state == STATE_CONNECTING) {
            state = STATE_CONNECTED;
        }
    }
}

static uint32_t pppos_output_cb(ppp_pcb *pcb, const void *data, uint32_t data_size, void *ctx) {
    (void)pcb;
    (void)ctx;

    const uint8_t *buffer = data;
    uint32_t n;
    uint32_t total = 0;
    while(total < data_size) {
        n = fifo_write(&fifo_tx, &buffer[total], data_size - total);
        if(n < (data_size - total)) {
            timer_restart(&timeout_tx);
            data_send();
        }
        total += n;
    }
    return data_size;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        dma_tx_ready = 1;
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        timer_restart(&timeout_rx);
        data_recv();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        timer_restart(&timeout_rx);
        data_recv();
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();

    state = STATE_DISCONNECTED;

    fifo_init(&fifo_tx);
    fifo_init(&fifo_rx);
    timer_init(&timeout_tx, 10);
    timer_init(&timeout_rx, 10);
    HAL_UART_Receive_DMA(&huart2, (uint8_t *)&dma_rx_buffer, sizeof(dma_rx_buffer));

    lwip_init();

    struct netif netif = {0};
    ppp_pcb *ppp = pppos_create(&netif, pppos_output_cb, pppos_link_status_cb, NULL);
    ppp_set_default(ppp);

    uint8_t buffer_rx[128];
    uint32_t buffer_len;

    uint32_t blink_counter = 1;
    timer_t blink;
    timer_init(&blink, 250);

    timer_t stats;
    timer_init(&stats, 1000);

    // CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // DWT->CYCCNT = 0;
    // DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    // uint32_t t0 = 0;
    // uint32_t t1 = 0;
    // uint32_t loop_max_us = 0;

    while(1) {
        // t1 = DWT->CYCCNT;
        // uint32_t dt_us = (t1 - t0) / (SystemCoreClock / 1000000);
        // if(dt_us > loop_max_us) {
        //     loop_max_us = dt_us;
        // }
        // t0 = DWT->CYCCNT;

        if(timer_timeout(&blink)) {
            timer_reset(&blink);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, blink_counter & 0x01);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, blink_counter & 0x02);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, blink_counter & 0x04);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, blink_counter & 0x08);
            blink_counter = ((blink_counter << 1) | (blink_counter >> 3)) & 0x0F;
        }

        if(timer_timeout(&stats)) {
            timer_reset(&stats);
            stats_display();
            // printf("main loop max iteration time: %lu us\n", (unsigned long)loop_max_us);
            // loop_max_us = 0;
        }

        switch(state) {
            case STATE_DISCONNECTED: {
                state = STATE_CONNECTING;
                ppp_connect(ppp, 0);
            } break;
            case STATE_CONNECTING: {

            } break;
            case STATE_CONNECTED: {
                state = STATE_LOOP;
                // printf("out ACCM (what we escape when sending): 0x%08lX\n",
                //        (unsigned long)ppp->lcp_gotoptions.asyncmap);
                // printf("in  ACCM (what peer escapes when sending to us): 0x%08lX\n",
                //        (unsigned long)ppp->lcp_hisoptions.asyncmap);

                ip_addr_t remote;
                IP4_ADDR(&remote, 192, 168, 7, 1);
                lwiperf_start_tcp_client(&remote, LWIPERF_TCP_PORT_DEFAULT, LWIPERF_DUAL, NULL,
                                         NULL);
                // lwiperf_start_tcp_server_default(NULL, NULL);
            } break;
            case STATE_LOOP: {

            } break;
        }

        if(timer_timeout(&timeout_tx)) {
            timer_reset(&timeout_tx);
            data_send();
        }

        if(timer_timeout(&timeout_rx)) {
            timer_reset(&timeout_rx);
            data_recv();
        }

        buffer_len = fifo_read(&fifo_rx, buffer_rx, sizeof(buffer_rx));
        if(buffer_len > 0) {
            pppos_input(ppp, buffer_rx, buffer_len);
        }

        sys_check_timeouts();
    }

    return 0;
}
