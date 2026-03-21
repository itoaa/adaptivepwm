/**
 * @file temperature_monitor.c
 * @brief Temperature monitoring implementation with hysteresis
 */

#include "temperature_monitor.h"
#include "adaptive_assert.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

bool TempMonitor_Init(TempMonitor_t* monitor)
{
    ADAPTIVE_ASSERT(monitor != NULL);
    
    if (monitor == NULL) return false;
    
    memset(monitor, 0, sizeof(TempMonitor_t));
    
    // Set default derating curve from config
    monitor->derating.temp_start = TEMP_WARNING_C;
    monitor->derating.temp_max = TEMP_CRITICAL_C;
    monitor->derating.power_max = 100.0f;
    monitor->allowed_power_percent = 100.0f;
    monitor->state = TEMP_STATE_NORMAL;
    
    return true;
}

void TempMonitor_Update(TempMonitor_t* monitor, float temp_c)
{
    ADAPTIVE_ASSERT(monitor != NULL);
    
    if (monitor == NULL) return;
    
    monitor->current_temp = temp_c;
    
    // Update max recorded
    if (temp_c > monitor->max_temp_recorded) {
        monitor->max_temp_recorded = temp_c;
    }
    
    // Simple moving average (exponential)
    monitor->avg_temp += MEASUREMENT_ALPHA * (temp_c - monitor->avg_temp);
    
    // State machine with hysteresis
    switch (monitor->state) {
        case TEMP_STATE_NORMAL:
            if (temp_c >= TEMP_WARNING_C) {
                monitor->state = TEMP_STATE_WARNING;
                monitor->warning_count++;
            }
            monitor->allowed_power_percent = 100.0f;
            break;
            
        case TEMP_STATE_WARNING:
            if (temp_c >= TEMP_CRITICAL_C) {
                monitor->state = TEMP_STATE_CRITICAL;
                monitor->critical_count++;
            } else if (temp_c <= TEMP_WARNING_C - TEMP_HYSTERESIS_C) {
                // Hysteresis: must drop below threshold
                monitor->state = TEMP_STATE_NORMAL;
            }
            // Linear derating from 100% to 50%
            monitor->allowed_power_percent = 100.0f - 
                ((temp_c - TEMP_WARNING_C) / (TEMP_CRITICAL_C - TEMP_WARNING_C)) * 50.0f;
            break;
            
        case TEMP_STATE_CRITICAL:
            if (temp_c >= TEMP_SHUTDOWN_C) {
                monitor->state = TEMP_STATE_SHUTDOWN;
                monitor->shutdown_count++;
            } else if (temp_c <= TEMP_CRITICAL_C - TEMP_HYSTERESIS_C) {
                // Hysteresis
                monitor->state = TEMP_STATE_WARNING;
            }
            monitor->allowed_power_percent = 50.0f;
            break;
            
        case TEMP_STATE_SHUTDOWN:
            monitor->allowed_power_percent = 0.0f;
            break;
    }
    
    monitor->derating_active = (monitor->state != TEMP_STATE_NORMAL);
}

float TempMonitor_GetAllowedPower(const TempMonitor_t* monitor)
{
    ADAPTIVE_ASSERT(monitor != NULL);
    
    if (monitor == NULL) return 0.0f;
    return monitor->allowed_power_percent;
}

bool TempMonitor_IsSafe(const TempMonitor_t* monitor)
{
    if (monitor == NULL) return false;
    return monitor->state < TEMP_STATE_CRITICAL;
}

bool TempMonitor_ShutdownRequired(const TempMonitor_t* monitor)
{
    if (monitor == NULL) return false;
    return monitor->state == TEMP_STATE_SHUTDOWN;
}

const char* TempMonitor_GetStateString(const TempMonitor_t* monitor)
{
    if (monitor == NULL) return "NULL";
    
    switch (monitor->state) {
        case TEMP_STATE_NORMAL:   return "NORMAL";
        case TEMP_STATE_WARNING:  return "WARNING";
        case TEMP_STATE_CRITICAL: return "CRITICAL";
        case TEMP_STATE_SHUTDOWN: return "SHUTDOWN";
        default:                  return "UNKNOWN";
    }
}

void TempMonitor_GetStatus(const TempMonitor_t* monitor, char* buffer, uint16_t size)
{
    if (monitor == NULL || buffer == NULL || size == 0) return;
    
    snprintf(buffer, size,
        "Temp: %.1fC (avg: %.1f, max: %.1f)\r\n"
        "State: %s\r\n"
        "Power: %.0f%% (%s)\r\n"
        "Warnings: %lu, Critical: %lu, Shutdown: %lu",
        monitor->current_temp, monitor->avg_temp, monitor->max_temp_recorded,
        TempMonitor_GetStateString(monitor),
        monitor->allowed_power_percent,
        monitor->derating_active ? "DERATING" : "NORMAL",
        (unsigned long)monitor->warning_count,
        (unsigned long)monitor->critical_count,
        (unsigned long)monitor->shutdown_count);
}
