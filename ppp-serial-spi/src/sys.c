#include <stm32f4xx_hal.h>

#include <lwip/sys.h>

sys_prot_t sys_arch_protect() {
    const sys_prot_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void sys_arch_unprotect(sys_prot_t pval) {
    __set_PRIMASK(pval);
}

uint32_t sys_now() {
    return HAL_GetTick();
}

uint32_t sys_jiffies() {
    return HAL_GetTick();
}
