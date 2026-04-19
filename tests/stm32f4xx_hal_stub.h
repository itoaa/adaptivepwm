/**
 * @file stm32f4xx_hal_stub.h
 * @brief Stub header for unit testing without STM32 HAL
 */

#ifndef STM32F4XX_HAL_STUB_H
#define STM32F4XX_HAL_STUB_H

#include <stdint.h>
#include <stdbool.h>

// Minimal type definitions for testing
typedef void* ADC_HandleTypeDef;
typedef void* DMA_HandleTypeDef;

// Stub functions
static inline void __HAL_RCC_RNG_CLK_ENABLE(void) {}
static inline void __HAL_RCC_RNG_CLK_DISABLE(void) {}

// Minimal HAL_GetTick for testing
extern uint32_t mock_hal_tick;
#define HAL_GetTick() (mock_hal_tick)

#endif // STM32F4XX_HAL_STUB_H