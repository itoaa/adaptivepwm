/**
 * @file stm32f4xx_hal_msp.c
 * @brief HAL MSP stubs — peripheral GPIO/DMA init is done in App HAL layers
 */
#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
