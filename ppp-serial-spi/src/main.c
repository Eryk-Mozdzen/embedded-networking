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

#define SPI_TRANSACTION_SIZE 1024

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi2;

typedef struct {
    UART_HandleTypeDef *huart;
    fifo_t fifo_tx;
    fifo_t fifo_rx;
    timer_t timeout_tx;
    timer_t timeout_rx;
    uint8_t dma_tx_buffer[8192];
    uint8_t dma_rx_buffer[8192];
    volatile uint8_t dma_tx_ready;
    volatile uint32_t dma_rx_pos;
} uart_t;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *GPIOx;
    uint16_t GPIO_Pin;
    fifo_t fifo_tx;
    fifo_t fifo_rx;
    timer_t timeout;
    uint8_t transaction_tx[SPI_TRANSACTION_SIZE];
    uint8_t transaction_rx[SPI_TRANSACTION_SIZE];
    volatile uint8_t transaction_ready;
} spi_t;

static uart_t uart1;
static uart_t uart2;
static spi_t spi2;

void SystemClock_Config();
void MX_GPIO_Init();
void MX_DMA_Init();
void MX_USART1_UART_Init();
void MX_USART2_UART_Init();
void MX_SPI2_Init();

static void uart_send(uart_t *uart) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if(uart->dma_tx_ready) {
        const uint32_t len =
            fifo_read(&uart->fifo_tx, uart->dma_tx_buffer, sizeof(uart->dma_tx_buffer));
        if(len > 0) {
            uart->dma_tx_ready = 0;
            HAL_UART_Transmit_DMA(uart->huart, uart->dma_tx_buffer, len);
        }
    }

    __set_PRIMASK(primask);
}

static void uart_recv(uart_t *uart) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t pos = sizeof(uart->dma_rx_buffer) - __HAL_DMA_GET_COUNTER(uart->huart->hdmarx);

    if(pos != uart->dma_rx_pos) {
        if(pos > uart->dma_rx_pos) {
            fifo_write(&uart->fifo_rx, &uart->dma_rx_buffer[uart->dma_rx_pos],
                       pos - uart->dma_rx_pos);
        } else {
            fifo_write(&uart->fifo_rx, &uart->dma_rx_buffer[uart->dma_rx_pos],
                       sizeof(uart->dma_rx_buffer) - uart->dma_rx_pos);
            if(pos > 0) {
                fifo_write(&uart->fifo_rx, uart->dma_rx_buffer, pos);
            }
        }
        uart->dma_rx_pos = pos;
    }

    __set_PRIMASK(primask);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == uart1.huart) {
        uart1.dma_tx_ready = 1;
    } else if(huart == uart2.huart) {
        uart2.dma_tx_ready = 1;
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == uart1.huart) {
        timer_restart(&uart1.timeout_rx);
        uart_recv(&uart1);
    } else if(huart == uart2.huart) {
        timer_restart(&uart2.timeout_rx);
        uart_recv(&uart2);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart == uart1.huart) {
        timer_restart(&uart1.timeout_rx);
        uart_recv(&uart1);
    } else if(huart == uart2.huart) {
        timer_restart(&uart2.timeout_rx);
        uart_recv(&uart2);
    }
}

static void uart_link(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;

    if(err_code == PPPERR_NONE) {
    }
}

static uint32_t uart_output(ppp_pcb *pcb, const void *data, uint32_t data_size, void *ctx) {
    (void)pcb;

    uart_t *uart = ctx;

    const uint8_t *buffer = data;
    uint32_t n;
    uint32_t total = 0;
    while(total < data_size) {
        n = fifo_write(&uart->fifo_tx, &buffer[total], data_size - total);
        if(n < (data_size - total)) {
            timer_restart(&uart->timeout_tx);
            uart_send(uart);
        }
        total += n;
    }
    return data_size;
}

static void uart_init(uart_t *uart, UART_HandleTypeDef *huart) {
    uart->huart = huart;
    uart->dma_tx_ready = 1;
    uart->dma_rx_pos = 0;
    fifo_init(&uart->fifo_tx);
    fifo_init(&uart->fifo_rx);
    timer_init(&uart->timeout_tx, 10);
    timer_init(&uart->timeout_rx, 10);
    HAL_UART_Receive_DMA(uart->huart, uart->dma_rx_buffer, sizeof(uart->dma_rx_buffer));
}

static void uart_loop(uart_t *uart, ppp_pcb *ppp) {
    if(timer_timeout(&uart->timeout_tx)) {
        timer_reset(&uart->timeout_tx);
        uart_send(uart);
    }

    if(timer_timeout(&uart->timeout_rx)) {
        timer_reset(&uart->timeout_rx);
        uart_recv(uart);
    }

    uint8_t buffer_rx[128];
    const uint32_t buffer_len = fifo_read(&uart->fifo_rx, buffer_rx, sizeof(buffer_rx));
    if(buffer_len > 0) {
        pppos_input(ppp, buffer_rx, buffer_len);
    }
}

static void spi_transaction(spi_t *spi) {
    if(spi->transaction_ready) {
        spi->transaction_ready = 0;

        const uint32_t len = fifo_read(&spi->fifo_tx, &spi->transaction_tx[sizeof(len)],
                                       SPI_TRANSACTION_SIZE - sizeof(len));

        spi->transaction_tx[0] = (len & 0x000000FF) >> 0;
        spi->transaction_tx[1] = (len & 0x0000FF00) >> 8;
        spi->transaction_tx[2] = (len & 0x00FF0000) >> 16;
        spi->transaction_tx[3] = (len & 0xFF000000) >> 24;

        HAL_GPIO_WritePin(spi->GPIOx, spi->GPIO_Pin, GPIO_PIN_RESET);
        HAL_SPI_TransmitReceive_DMA(spi->hspi, spi->transaction_tx, spi->transaction_rx,
                                    SPI_TRANSACTION_SIZE);
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if(hspi == spi2.hspi) {
        HAL_GPIO_WritePin(spi2.GPIOx, spi2.GPIO_Pin, GPIO_PIN_SET);

        uint32_t len =
            (((uint32_t)spi2.transaction_rx[0]) << 0) | (((uint32_t)spi2.transaction_rx[1]) << 8) |
            (((uint32_t)spi2.transaction_rx[2]) << 16) | (((uint32_t)spi2.transaction_rx[3]) << 24);

        if(len > (SPI_TRANSACTION_SIZE - sizeof(len))) {
            len = SPI_TRANSACTION_SIZE - sizeof(len);
        }

        fifo_write(&spi2.fifo_rx, &spi2.transaction_rx[sizeof(len)], len);

        spi2.transaction_ready = 1;
    }
}

static void spi_link(ppp_pcb *pcb, int err_code, void *ctx) {
    (void)pcb;
    (void)ctx;

    if(err_code == PPPERR_NONE) {
    }
}

static uint32_t spi_output(ppp_pcb *pcb, const void *data, uint32_t data_size, void *ctx) {
    (void)pcb;
    spi_t *spi = ctx;
    return fifo_write(&spi->fifo_tx, data, data_size);
}

static void spi_init(spi_t *spi, SPI_HandleTypeDef *hspi, GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    spi->hspi = hspi;
    spi->GPIOx = GPIOx;
    spi->GPIO_Pin = GPIO_Pin;
    spi->transaction_ready = 1;
    fifo_init(&spi->fifo_tx);
    fifo_init(&spi->fifo_rx);
    timer_init(&spi->timeout, 10);
    HAL_GPIO_WritePin(spi->GPIOx, spi->GPIO_Pin, GPIO_PIN_SET);
}

static void spi_loop(spi_t *spi, ppp_pcb *ppp) {
    if(timer_timeout(&spi->timeout)) {
        timer_reset(&spi->timeout);
        spi_transaction(spi);
    }

    uint8_t buffer_rx[128];
    const uint32_t buffer_len = fifo_read(&spi->fifo_rx, buffer_rx, sizeof(buffer_rx));
    if(buffer_len > 0) {
        pppos_input(ppp, buffer_rx, buffer_len);
    }
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
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_SPI2_Init();

    uart_init(&uart1, &huart1);
    uart_init(&uart2, &huart2);
    spi_init(&spi2, &hspi2, GPIOB, GPIO_PIN_12);

    lwip_init();

    struct netif netif1 = {0};
    struct netif netif2 = {0};
    struct netif netif3 = {0};

    ppp_pcb *ppp1 = pppos_create(&netif1, uart_output, uart_link, &uart1);
    ppp_pcb *ppp2 = pppos_create(&netif2, uart_output, uart_link, &uart2);
    ppp_pcb *ppp3 = pppos_create(&netif3, spi_output, spi_link, &spi2);

    netif_set_default(&netif2);

    ppp_connect(ppp1, 0);
    ppp_connect(ppp2, 0);
    ppp_connect(ppp3, 0);

    uint32_t blink_counter = 1;
    timer_t blink;
    timer_init(&blink, 250);

    telnet_t telnet = {0};
    struct tcp_pcb *telnet_pcb = tcp_new();
    tcp_bind(telnet_pcb, IP_ADDR_ANY, 23);
    telnet_pcb = tcp_listen(telnet_pcb);
    tcp_arg(telnet_pcb, &telnet);
    tcp_accept(telnet_pcb, telnet_accept);

    while(1) {
        if(timer_timeout(&blink)) {
            timer_reset(&blink);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, blink_counter & 0x01);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, blink_counter & 0x02);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, blink_counter & 0x04);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, blink_counter & 0x08);
            blink_counter = ((blink_counter << 1) | (blink_counter >> 3)) & 0x0F;
        }

        uart_loop(&uart1, ppp1);
        uart_loop(&uart2, ppp2);
        spi_loop(&spi2, ppp3);

        sys_check_timeouts();
    }

    return 0;
}
