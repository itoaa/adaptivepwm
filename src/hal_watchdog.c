/**
 * @file hal_watchdog.c
 * @brief Watchdog implementation
 */

#include "hal_watchdog.h"

static IWDG_HandleTypeDef iwdg_handle;
static uint32_t timeout_ms = 1000;

// Prescaler and reload values for different timeouts
// At 32kHz LSI, with prescaler /32 = 1kHz tick rate
static const struct {
    uint32_t prescaler;
    uint32_t reload;
    uint32_t timeout;
} wdg_configs[] = {
    {IWDG_PRESCALER_4,   80,    10},    // ~10ms
    {IWDG_PRESCALER_32,  100,   100},   // ~100ms
    {IWDG_PRESCALER_64,  250,   500},   // ~500ms
    {IWDG_PRESCALER_64,  500,   1000},  // ~1s
    {IWDG_PRESCALER_128, 500,   2000},  // ~2s
    {IWDG_PRESCALER_256, 625,   5000},  // ~5s
};

bool Adaptive_WDG_Init(uint8_t timeout_level)
{
    if (timeout_level > 5) timeout_level = 3;  // Default 1s
    
    iwdg_handle.Instance = IWDG;
    iwdg_handle.Init.Prescaler = wdg_configs[timeout_level].prescaler;
    iwdg_handle.Init.Reload = wdg_configs[timeout_level].reload;
    
    timeout_ms = wdg_configs[timeout_level].timeout;
    
    return HAL_IWDG_Init(&iwdg_handle) == HAL_OK;
}

void Adaptive_WDG_Refresh(void)
{
    HAL_IWDG_Refresh(&iwdg_handle);
}

bool Adaptive_WDG_WasReset(void)
{
    return (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET);
}

uint32_t Adaptive_WDG_GetTimeout(void)
{
    return timeout_ms;
}
