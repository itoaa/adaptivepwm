/**
 * @file current_protection.c
 * @brief Hardware current protection implementation
 */

#include "current_protection.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

bool CurrentProtection_Init(CurrentProtection_t* prot, float threshold)
{
    if (prot == NULL) return false;
    
    memset(prot, 0, sizeof(CurrentProtection_t));
    prot->threshold_warning = threshold * 0.8f;
    prot->threshold_shutdown = threshold;
    prot->enabled = false;
    
    // Note: STM32F401 doesn't have dedicated COMP peripheral
    // Using ADC threshold detection instead
    
    return true;
}

void CurrentProtection_Enable(CurrentProtection_t* prot)
{
    if (prot != NULL) {
        prot->enabled = true;
    }
}

void CurrentProtection_Disable(CurrentProtection_t* prot)
{
    if (prot != NULL) {
        prot->enabled = false;
    }
}

void CurrentProtection_SetThreshold(CurrentProtection_t* prot, float warning, float shutdown)
{
    if (prot == NULL) return;
    
    prot->threshold_warning = warning;
    prot->threshold_shutdown = shutdown;
}

OC_Level_t CurrentProtection_Check(CurrentProtection_t* prot, float current)
{
    if (prot == NULL || !prot->enabled) return OC_LEVEL_NONE;
    
    float abs_current = current > 0 ? current : -current;
    
    if (abs_current >= prot->threshold_shutdown) {
        prot->last_level = OC_LEVEL_SHUTDOWN;
        prot->trip_count++;
        return OC_LEVEL_SHUTDOWN;
    } else if (abs_current >= prot->threshold_warning) {
        prot->last_level = OC_LEVEL_WARNING;
        return OC_LEVEL_WARNING;
    }
    
    prot->last_level = OC_LEVEL_NONE;
    return OC_LEVEL_NONE;
}

bool CurrentProtection_GetStatus(const CurrentProtection_t* prot, char* buffer, uint16_t size)
{
    if (prot == NULL || buffer == NULL || size == 0) return false;
    
    const char* level_str = "OK";
    switch (prot->last_level) {
        case OC_LEVEL_WARNING: level_str = "WARNING"; break;
        case OC_LEVEL_SHUTDOWN: level_str = "SHUTDOWN"; break;
        default: break;
    }
    
    snprintf(buffer, size,
        "Current Protection:\r\n"
        "  State: %s\r\n"
        "  Enabled: %s\r\n"
        "  Warning: %.2f A\r\n"
        "  Shutdown: %.2f A\r\n"
        "  Trips: %lu",
        level_str,
        prot->enabled ? "YES" : "NO",
        prot->threshold_warning,
        prot->threshold_shutdown,
        (unsigned long)prot->trip_count);
    
    return true;
}
