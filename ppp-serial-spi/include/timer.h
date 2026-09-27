#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

typedef struct {
    uint32_t start;
    uint32_t period;
} timer_t;

void timer_init(timer_t *timer, const uint32_t period);
void timer_reset(timer_t *timer);
void timer_restart(timer_t *timer);
uint8_t timer_timeout(timer_t *timer);

#endif
