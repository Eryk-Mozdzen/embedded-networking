#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define timer_init(timer, period)                                                                  \
    do {                                                                                           \
        (timer)->_start = HAL_GetTick();                                                           \
        (timer)->_period = (period);                                                               \
    } while(0)
#define timer_timeout(timer) ((HAL_GetTick() - (timer)->_start) >= (timer)->_period)
#define timer_reset(timer)                                                                         \
    do {                                                                                           \
        (timer)->_start += (timer)->_period;                                                       \
    } while(0)

typedef struct {
    uint32_t _start;
    uint32_t _period;
} timer_t;

#endif
