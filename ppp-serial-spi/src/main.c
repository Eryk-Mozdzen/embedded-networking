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
static state_t state;

static volatile uint8_t send_ready = 1;
static volatile uint8_t recv_byte;

void SystemClock_Config();
void MX_GPIO_Init();
void MX_USART2_UART_Init();

static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;

    if(err_code == PPPERR_NONE) {
        if(state == STATE_CONNECTING) {
            state = STATE_CONNECTED;
        }
    }
}

static uint32_t ppp_output_cb(ppp_pcb *pcb, const void *data, uint32_t data_size, void *ctx) {
    (void)pcb;
    (void)ctx;
    return fifo_write(&fifo_tx, data, data_size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        send_ready = 1;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        fifo_write(&fifo_rx, (uint8_t *)&recv_byte, 1);
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&recv_byte, 1);
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    state = STATE_DISCONNECTED;

    fifo_init(&fifo_tx);
    fifo_init(&fifo_rx);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&recv_byte, 1);

    lwip_init();

    struct netif netif = {0};
    ppp_pcb *ppp = pppos_create(&netif, ppp_output_cb, ppp_link_status_cb, NULL);
    ppp_set_default(ppp);

    uint8_t buffer_tx[1024];
    uint8_t buffer_rx[1024];
    uint32_t buffer_len;

    uint32_t blink_counter = 1;

    timer_t blink;

    timer_init(&blink, 250);

    while(1) {
        if(timer_timeout(&blink)) {
            timer_reset(&blink);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, blink_counter & 0x01);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, blink_counter & 0x02);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, blink_counter & 0x04);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, blink_counter & 0x08);
            blink_counter = ((blink_counter << 1) | (blink_counter >> 3)) & 0x0F;
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
                lwiperf_start_tcp_server_default(NULL, NULL);
            } break;
            case STATE_LOOP: {

            } break;
        }

        buffer_len = fifo_read(&fifo_rx, buffer_rx, sizeof(buffer_rx));
        if(buffer_len > 0) {
            pppos_input(ppp, buffer_rx, buffer_len);
        }

        if(send_ready) {
            buffer_len = fifo_read(&fifo_tx, buffer_tx, sizeof(buffer_tx));
            if(buffer_len > 0) {
                send_ready = 0;
                HAL_UART_Transmit_IT(&huart2, buffer_tx, buffer_len);
            }
        }

        sys_check_timeouts();
    }

    return 0;
}
