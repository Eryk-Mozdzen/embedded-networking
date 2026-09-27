#include <stm32f4xx_hal.h>

#include "timer.h"

void timer_init(timer_t *timer, const uint32_t period) {
    timer->start = HAL_GetTick();
    timer->period = period;
}

void timer_reset(timer_t *timer) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    timer->start += timer->period;

    __set_PRIMASK(primask);
}

void timer_restart(timer_t *timer) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    timer->start = HAL_GetTick();

    __set_PRIMASK(primask);
}

uint8_t timer_timeout(timer_t *timer) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t delta = HAL_GetTick() - timer->start;

    __set_PRIMASK(primask);

    return (delta >= timer->period);
}
