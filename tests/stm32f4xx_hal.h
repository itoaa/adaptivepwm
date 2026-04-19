/* Stub for STM32 HAL header for testing */
#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H
#include <stdint.h>
#include <stdbool.h>

typedef struct { void *Instance; } ADC_HandleTypeDef;
typedef struct { void *Instance; } DMA_HandleTypeDef;
typedef struct { void *Instance; } UART_HandleTypeDef;

static inline void __HAL_RCC_RNG_CLK_ENABLE(void) {}
static inline void __HAL_RCC_RNG_CLK_DISABLE(void) {}

extern uint32_t mock_hal_tick_val;
static inline uint32_t HAL_GetTick(void) { return mock_hal_tick_val; }

// Minimal TIM handle for pwm
typedef struct { void *Instance; } TIM_HandleTypeDef;

// Minimal flash defines
#define FLASH_SECTOR_7 7
#define FLASH_SECTOR_6 6

#endif