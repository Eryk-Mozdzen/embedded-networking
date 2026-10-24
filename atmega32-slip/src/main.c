#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include <slipdev.h>
#include <timer.h>
#include <uip.h>

#include "clock-arch.h"

#define UART_BAUDRATE   115200UL
#define UART_UBRR_VALUE ((F_CPU / (8UL * UART_BAUDRATE)) - 1)

typedef struct {
    volatile uint8_t buffer[128];
    volatile uint16_t rd;
    volatile uint16_t wr;
} fifo_t;

static fifo_t fifo_tx;
static fifo_t fifo_rx;
static volatile clock_time_t counter_ms = 0;

static void fifo_init(fifo_t *fifo) {
    fifo->rd = 0;
    fifo->wr = 0;
}

static inline uint16_t fifo_write(fifo_t *fifo, const uint8_t *src, uint16_t src_len) {
    uint16_t num = 0;
    const uint8_t sreg = SREG;
    cli();
    while(num < src_len) {
        uint16_t next = fifo->wr + 1;
        if(next >= sizeof(fifo->buffer)) {
            next = 0;
        }
        if(next == fifo->rd) {
            break;
        }
        fifo->buffer[fifo->wr] = src[num];
        fifo->wr = next;
        num++;
    }
    SREG = sreg;
    return num;
}

static inline uint16_t fifo_read(fifo_t *fifo, uint8_t *dst, const uint16_t dst_capacity) {
    uint16_t num = 0;
    const uint8_t sreg = SREG;
    cli();
    while((fifo->rd != fifo->wr) && (num < dst_capacity)) {
        dst[num] = fifo->buffer[fifo->rd];
        fifo->rd++;
        num++;
        if(fifo->rd >= sizeof(fifo->buffer)) {
            fifo->rd = 0;
        }
    }
    SREG = sreg;
    return num;
}

void slipdev_char_put(u8_t c) {
    if(fifo_write(&fifo_tx, &c, sizeof(c)) > 0) {
        UCSRB |= (1 << UDRIE);
    }
}

u8_t slipdev_char_poll(u8_t *c) {
    return (fifo_read(&fifo_rx, c, sizeof(*c)) > 0);
}

clock_time_t clock_time() {
    clock_time_t value;
    const uint8_t sreg = SREG;
    cli();
    value = counter_ms;
    SREG = sreg;
    return value;
}

ISR(USART_RXC_vect) {
    const uint8_t byte = UDR;
    fifo_write(&fifo_rx, &byte, sizeof(byte));
}

ISR(USART_UDRE_vect) {
    uint8_t byte;
    if(fifo_read(&fifo_tx, &byte, sizeof(byte)) > 0) {
        UDR = byte;
    } else {
        UCSRB &= ~(1 << UDRIE);
    }
}

ISR(TIMER0_COMP_vect) {
    counter_ms++;
}

static uint8_t cmd_parse(char *buffer, char **argv, const uint8_t argv_capacity) {
    uint8_t argc = 0;

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

void app_call() {
    if(uip_newdata()) {
        const uint16_t data_len = uip_datalen();
        char *data = uip_appdata;
        data[data_len] = '\0';

        char *argv[8];
        const uint8_t argc = cmd_parse(data, argv, 8);

        if((strcmp(argv[0], "pwm") == 0) && (argc == 2)) {
            const int16_t value = strtol(argv[1], NULL, 10);

            if((value >= 0) && (value <= 100)) {
                OCR1A = 2000 + (20 * value);
            }
        }
    }
}

int main() {
    fifo_init(&fifo_tx);
    fifo_init(&fifo_rx);

    UBRRH = (uint8_t)(UART_UBRR_VALUE >> 8);
    UBRRL = (uint8_t)(UART_UBRR_VALUE);
    UCSRA = (1 << U2X);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);

    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
    OCR0 = 249;
    TIMSK |= (1 << OCIE0);

    DDRD |= (1 << PD5);
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = 39999;
    OCR1A = 2000;

    slipdev_init();
    uip_init();

    uip_ipaddr_t ip;
    uip_ipaddr(&ip, 192, 168, 9, 1);
    uip_sethostaddr(&ip);

    uip_ipaddr_t netmask;
    uip_ipaddr(&netmask, 255, 255, 255, 0);
    uip_setnetmask(&netmask);

    struct timer timer_periodic;
    timer_set(&timer_periodic, 10);

    uip_listen(HTONS(23));

    sei();

    while(1) {
        uip_len = slipdev_poll();
        if(uip_len > 0) {
            uip_input();
            if(uip_len > 0) {
                slipdev_send();
            }
        }

        if(timer_expired(&timer_periodic)) {
            timer_reset(&timer_periodic);
            for(uint8_t i = 0; i < UIP_CONNS; i++) {
                uip_periodic(i);
                if(uip_len > 0) {
                    slipdev_send();
                }
            }
        }
    }

    return 0;
}
