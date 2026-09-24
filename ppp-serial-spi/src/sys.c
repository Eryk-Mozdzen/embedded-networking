#include <stdint.h>

#include <stm32f4xx_hal.h>

#include <lwip/sys.h>

sys_prot_t sys_arch_protect() {
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
}

uint32_t sys_now() {
    return HAL_GetTick();
}

uint32_t sys_jiffies() {
    return HAL_GetTick();
}
