/**
 * @file hal_watchdog.h
 * @brief Independent Watchdog (IWDG) HAL
 */

#ifndef HAL_WATCHDOG_H
#define HAL_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// Watchdog timeout levels (at 32kHz LSI)
// Maps to prescaler/reload values
enum {
    WDG_LEVEL_10MS = 0,   // ~10ms timeout
    WDG_LEVEL_100MS,      // ~100ms timeout  
    WDG_LEVEL_500MS,      // ~500ms timeout
    WDG_LEVEL_1S,         // ~1 second
    WDG_LEVEL_2S,         // ~2 seconds
    WDG_LEVEL_5S,         // ~5 seconds
    WDG_LEVEL_COUNT
};

// Helper macro to select level from milliseconds
#define WDG_MS_TO_LEVEL(ms) \
    ((ms) <= 20 ? WDG_LEVEL_10MS : \
     (ms) <= 200 ? WDG_LEVEL_100MS : \
     (ms) <= 1000 ? WDG_LEVEL_500MS : \
     (ms) <= 2000 ? WDG_LEVEL_1S : \
     (ms) <= 4000 ? WDG_LEVEL_2S : WDG_LEVEL_5S)

bool Adaptive_WDG_Init(uint8_t timeout_level);
void Adaptive_WDG_Refresh(void);
bool Adaptive_WDG_WasReset(void);
uint32_t Adaptive_WDG_GetTimeout(void);

#endif // HAL_WATCHDOG_H
