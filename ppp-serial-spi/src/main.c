#include <stm32f4xx_hal.h>

void SystemClock_Config();

int main() {
    HAL_Init();

    SystemClock_Config();

    while(1) {
    }

    return 0;
}
