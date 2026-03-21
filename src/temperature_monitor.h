/**
 * @file temperature_monitor.h
 * @brief Temperature monitoring and thermal management with hysteresis
 */

#ifndef TEMPERATURE_MONITOR_H
#define TEMPERATURE_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// Thermal derating curve
typedef struct {
    float temp_start;       // Start derating at this temp
    float temp_max;         // Maximum temp (0% power)
    float power_max;        // Maximum power at low temp
} ThermalDerating_t;

typedef struct {
    float current_temp;
    float max_temp_recorded;
    float avg_temp;
    uint32_t warning_count;
    uint32_t critical_count;
    uint32_t shutdown_count;
    ThermalDerating_t derating;
    bool derating_active;
    float allowed_power_percent;
    
    // State machine for hysteresis
    enum {
        TEMP_STATE_NORMAL,
        TEMP_STATE_WARNING,
        TEMP_STATE_CRITICAL,
        TEMP_STATE_SHUTDOWN
    } state;
} TempMonitor_t;

bool TempMonitor_Init(TempMonitor_t* monitor);
void TempMonitor_Update(TempMonitor_t* monitor, float temp_c);
float TempMonitor_GetAllowedPower(const TempMonitor_t* monitor);
bool TempMonitor_IsSafe(const TempMonitor_t* monitor);
bool TempMonitor_ShutdownRequired(const TempMonitor_t* monitor);
void TempMonitor_GetStatus(const TempMonitor_t* monitor, char* buffer, uint16_t size);
const char* TempMonitor_GetStateString(const TempMonitor_t* monitor);

#endif // TEMPERATURE_MONITOR_H
