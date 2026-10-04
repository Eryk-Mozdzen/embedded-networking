#include <string.h>

#include <stm32f4xx_hal.h>

#include <lwip/apps/lwiperf.h>
#include <lwip/init.h>
#include <lwip/tcp.h>
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
extern SPI_HandleTypeDef hspi2;

static struct {
    fifo_t fifo_tx;
    fifo_t fifo_rx;
    timer_t timeout_tx;
    timer_t timeout_rx;
    state_t state;
    volatile uint8_t dma_tx_ready;
    volatile uint8_t dma_tx_buffer[8192];
    volatile uint32_t dma_rx_pos;
    volatile uint8_t dma_rx_buffer[8192];
} ppp_uart;

void SystemClock_Config();
void MX_GPIO_Init();
void MX_DMA_Init();
void MX_USART2_UART_Init();
void MX_SPI2_Init();

static void ppp_uart_data_send() {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if(ppp_uart.dma_tx_ready) {
        const uint32_t len = fifo_read(&ppp_uart.fifo_tx, (uint8_t *)ppp_uart.dma_tx_buffer,
                                       sizeof(ppp_uart.dma_tx_buffer));
        if(len > 0) {
            ppp_uart.dma_tx_ready = 0;
            HAL_UART_Transmit_DMA(&huart2, (uint8_t *)ppp_uart.dma_tx_buffer, len);
        }
    }

    __set_PRIMASK(primask);
}

static void ppp_uart_data_recv() {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t pos = sizeof(ppp_uart.dma_rx_buffer) - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

    if(pos != ppp_uart.dma_rx_pos) {
        if(pos > ppp_uart.dma_rx_pos) {
            fifo_write(&ppp_uart.fifo_rx, (uint8_t *)&ppp_uart.dma_rx_buffer[ppp_uart.dma_rx_pos],
                       pos - ppp_uart.dma_rx_pos);
        } else {
            fifo_write(&ppp_uart.fifo_rx, (uint8_t *)&ppp_uart.dma_rx_buffer[ppp_uart.dma_rx_pos],
                       sizeof(ppp_uart.dma_rx_buffer) - ppp_uart.dma_rx_pos);
            if(pos > 0) {
                fifo_write(&ppp_uart.fifo_rx, (uint8_t *)ppp_uart.dma_rx_buffer, pos);
            }
        }
        ppp_uart.dma_rx_pos = pos;
    }

    __set_PRIMASK(primask);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        ppp_uart.dma_tx_ready = 1;
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        timer_restart(&ppp_uart.timeout_rx);
        ppp_uart_data_recv();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == &huart2) {
        timer_restart(&ppp_uart.timeout_rx);
        ppp_uart_data_recv();
    }
}

static void ppp_uart_link(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;

    if(err_code == PPPERR_NONE) {
        if(ppp_uart.state == STATE_CONNECTING) {
            ppp_uart.state = STATE_CONNECTED;
        }
    }
}

static uint32_t ppp_uart_output(ppp_pcb *pcb, const void *data, uint32_t data_size, void *ctx) {
    (void)pcb;
    (void)ctx;

    const uint8_t *buffer = data;
    uint32_t n;
    uint32_t total = 0;
    while(total < data_size) {
        n = fifo_write(&ppp_uart.fifo_tx, &buffer[total], data_size - total);
        if(n < (data_size - total)) {
            timer_restart(&ppp_uart.timeout_tx);
            ppp_uart_data_send();
        }
        total += n;
    }
    return data_size;
}

#define PPP_SPI_TRANSACTION_SIZE 1024

static struct {
    fifo_t fifo_tx;
    fifo_t fifo_rx;
    timer_t timeout;
    state_t state;
    uint8_t transaction_tx[PPP_SPI_TRANSACTION_SIZE];
    uint8_t transaction_rx[PPP_SPI_TRANSACTION_SIZE];
    volatile uint8_t transaction_ready;
} ppp_spi;

static void ppp_spi_transaction() {
    if(ppp_spi.transaction_ready) {
        ppp_spi.transaction_ready = 0;

        const uint32_t len = fifo_read(&ppp_spi.fifo_tx, &ppp_spi.transaction_tx[sizeof(len)],
                                       PPP_SPI_TRANSACTION_SIZE - sizeof(len));

        ppp_spi.transaction_tx[0] = (len & 0x000000FF) >> 0;
        ppp_spi.transaction_tx[1] = (len & 0x0000FF00) >> 8;
        ppp_spi.transaction_tx[2] = (len & 0x00FF0000) >> 16;
        ppp_spi.transaction_tx[3] = (len & 0xFF000000) >> 24;

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
        HAL_SPI_TransmitReceive_DMA(&hspi2, ppp_spi.transaction_tx, ppp_spi.transaction_rx,
                                    PPP_SPI_TRANSACTION_SIZE);
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if(hspi == &hspi2) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

        uint32_t len = (((uint32_t)ppp_spi.transaction_rx[0]) << 0) |
                       (((uint32_t)ppp_spi.transaction_rx[1]) << 8) |
                       (((uint32_t)ppp_spi.transaction_rx[2]) << 16) |
                       (((uint32_t)ppp_spi.transaction_rx[3]) << 24);

        if(len > (PPP_SPI_TRANSACTION_SIZE - sizeof(len))) {
            len = PPP_SPI_TRANSACTION_SIZE - sizeof(len);
        }

        fifo_write(&ppp_spi.fifo_rx, &ppp_spi.transaction_rx[sizeof(len)], len);

        ppp_spi.transaction_ready = 1;
    }
}

static void ppp_spi_link(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;

    if(err_code == PPPERR_NONE) {
        if(ppp_spi.state == STATE_CONNECTING) {
            ppp_spi.state = STATE_CONNECTED;
        }
    }
}

static uint32_t ppp_spi_output(ppp_pcb *pcb, const void *data, uint32_t data_size, void *ctx) {
    (void)pcb;
    (void)ctx;
    return fifo_write(&ppp_spi.fifo_tx, data, data_size);
}

typedef struct {
    char buffer[128];
    uint32_t len;
} telnet_t;

static uint32_t telnet_parse(char *buffer, char **argv, const uint32_t argv_capacity) {
    uint32_t argc = 0;

    while(*buffer && (argc < argv_capacity)) {
        while(isspace((unsigned char)*buffer)) {
            buffer++;
        }

        if(*buffer == '\0') {
            break;
        }

        argv[argc] = buffer;
        argc++;

        while(*buffer && !isspace((unsigned char)*buffer)) {
            buffer++;
        }

        if(*buffer) {
            *buffer = '\0';
            buffer++;
        }
    }

    return argc;
}

static void telnet_transmit(struct tcp_pcb *pcb, const char *message) {
    tcp_write(pcb, message, strlen(message), TCP_WRITE_FLAG_COPY);
}

static err_t telnet_receive(void *arg, struct tcp_pcb *pcb, struct pbuf *message, err_t err) {
    telnet_t *telnet = arg;

    if(message == NULL) {
        tcp_close(pcb);
        return ERR_OK;
    }

    for(uint32_t i = 0; (i < message->len) && (telnet->len < sizeof(telnet->buffer)); i++) {
        const char byte = ((uint8_t *)message->payload)[i];

        if(byte == '\n') {
            telnet->buffer[telnet->len] = '\0';

            char *argv[16];
            const uint32_t argc = telnet_parse(telnet->buffer, argv, 16);

            if(strcmp(argv[0], "calib_full") == 0) {
                telnet_transmit(pcb, "OK\r\n");
            } else if(strcmp(argv[0], "calib_curr") == 0) {
                telnet_transmit(pcb, "OK\r\n");
            } else if(strcmp(argv[0], "calib_mot") == 0) {
                telnet_transmit(pcb, "OK\r\n");
            } else if((strcmp(argv[0], "tr") == 0) && (argc == 2)) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "    torque setpoint = Nm\n\rOK\n\r");
                telnet_transmit(pcb, buffer);
            } else if(strcmp(argv[0], "stop") == 0) {
                telnet_transmit(pcb, "OK\r\n");
            }

            telnet->len = 0;
        }

        if((byte != '\n') && (byte != '\r')) {
            telnet->buffer[telnet->len] = byte;
            telnet->len++;
        }
    }

    tcp_recved(pcb, message->tot_len);
    pbuf_free(message);

    return ERR_OK;
}

static err_t telnet_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)err;

    tcp_arg(pcb, arg);
    tcp_recv(pcb, telnet_receive);

    const char *header = "------------------------------------\n\r     PPP example "__DATE__
                         " "__TIME__
                         "\r\n------------------------------------\n\r";
    tcp_write(pcb, header, strlen(header), TCP_WRITE_FLAG_COPY);

    return ERR_OK;
}

int main() {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_SPI2_Init();

    ppp_uart.dma_tx_ready = 1;
    ppp_uart.dma_rx_pos = 0;
    ppp_uart.state = STATE_DISCONNECTED;
    fifo_init(&ppp_uart.fifo_tx);
    fifo_init(&ppp_uart.fifo_rx);
    timer_init(&ppp_uart.timeout_tx, 10);
    timer_init(&ppp_uart.timeout_rx, 10);
    HAL_UART_Receive_DMA(&huart2, (uint8_t *)&ppp_uart.dma_rx_buffer,
                         sizeof(ppp_uart.dma_rx_buffer));

    ppp_spi.transaction_ready = 1;
    ppp_spi.state = STATE_DISCONNECTED;
    fifo_init(&ppp_spi.fifo_tx);
    fifo_init(&ppp_spi.fifo_rx);
    timer_init(&ppp_spi.timeout, 10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

    lwip_init();

    struct netif netif1 = {0};
    ppp_pcb *ppp1 = pppos_create(&netif1, ppp_uart_output, ppp_uart_link, NULL);

    struct netif netif2 = {0};
    ppp_pcb *ppp2 = pppos_create(&netif2, ppp_spi_output, ppp_spi_link, NULL);

    ppp_set_default(ppp2);

    uint8_t buffer_rx[128];
    uint32_t buffer_len;

    uint32_t blink_counter = 1;
    timer_t blink;
    timer_init(&blink, 250);

    // timer_t stats;
    // timer_init(&stats, 1000);

    telnet_t telnet = {0};
    struct tcp_pcb *telnet_pcb = tcp_new();
    tcp_bind(telnet_pcb, IP_ADDR_ANY, 23);
    telnet_pcb = tcp_listen(telnet_pcb);
    tcp_arg(telnet_pcb, &telnet);
    tcp_accept(telnet_pcb, telnet_accept);

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

        // if(timer_timeout(&stats)) {
        //     timer_reset(&stats);
        //     stats_display();
        //     printf("main loop max iteration time: %lu us\n", (unsigned long)loop_max_us);
        //     loop_max_us = 0;
        // }

        switch(ppp_uart.state) {
            case STATE_DISCONNECTED: {
                ppp_uart.state = STATE_CONNECTING;
                ppp_connect(ppp1, 0);
            } break;
            case STATE_CONNECTING: {

            } break;
            case STATE_CONNECTED: {
                ppp_uart.state = STATE_LOOP;
                // printf("out ACCM (what we escape when sending): 0x%08lX\n",
                //        (unsigned long)ppp->lcp_gotoptions.asyncmap);
                // printf("in  ACCM (what peer escapes when sending to us): 0x%08lX\n",
                //        (unsigned long)ppp->lcp_hisoptions.asyncmap);

                // ip_addr_t remote;
                // IP4_ADDR(&remote, 192, 168, 4, 2);
                // lwiperf_start_tcp_client(&remote, LWIPERF_TCP_PORT_DEFAULT, LWIPERF_CLIENT, NULL,
                //                          NULL);
                // lwiperf_start_tcp_server_default(NULL, NULL);
            } break;
            case STATE_LOOP: {

            } break;
        }

        if(timer_timeout(&ppp_uart.timeout_tx)) {
            timer_reset(&ppp_uart.timeout_tx);
            ppp_uart_data_send();
        }

        if(timer_timeout(&ppp_uart.timeout_rx)) {
            timer_reset(&ppp_uart.timeout_rx);
            ppp_uart_data_recv();
        }

        buffer_len = fifo_read(&ppp_uart.fifo_rx, buffer_rx, sizeof(buffer_rx));
        if(buffer_len > 0) {
            pppos_input(ppp1, buffer_rx, buffer_len);
        }

        switch(ppp_spi.state) {
            case STATE_DISCONNECTED: {
                ppp_spi.state = STATE_CONNECTING;
                ppp_connect(ppp2, 0);
            } break;
            case STATE_CONNECTING: {

            } break;
            case STATE_CONNECTED: {
                ppp_spi.state = STATE_LOOP;

                // ip_addr_t remote;
                // IP4_ADDR(&remote, 192, 168, 0, 17);
                // lwiperf_start_tcp_client(&remote, LWIPERF_TCP_PORT_DEFAULT, LWIPERF_CLIENT, NULL,
                //                          NULL);
                lwiperf_start_tcp_server_default(NULL, NULL);
            } break;
            case STATE_LOOP: {

            } break;
        }

        if(timer_timeout(&ppp_spi.timeout)) {
            timer_reset(&ppp_spi.timeout);
            ppp_spi_transaction();
        }

        buffer_len = fifo_read(&ppp_spi.fifo_rx, buffer_rx, sizeof(buffer_rx));
        if(buffer_len > 0) {
            pppos_input(ppp2, buffer_rx, buffer_len);
        }

        sys_check_timeouts();
    }

    return 0;
}
