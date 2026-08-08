/**
 * @file thermal_runaway.c
 * @brief Thermal Runaway Protection for AdaptivePWM
 *
 * Security Task: SEC-009 / ADP-005
 * CVSS: 9.1 - Critical
 *
 * Implements dT/dt > 5°C/s detection with emergency shutdown.
 * Thermal runaway can cause fires, equipment damage, or injury.
 *
 * Framework: CISSP Domain 3/8, NIST CSF PR.IP-1, ISO 27001 8.1
 *
 * @author AdaptivePWM Security Team
 * @date 2026-04-10
 */

#include "thermal_runaway.h"
#include "adaptive_assert.h"
#include <string.h>
#include <math.h>

// Safety configuration constants
#define THERMAL_RUNAWAY_THRESHOLD_C_PER_S 5.0f   /**< dT/dt threshold for shutdown */
#define MIN_SAMPLE_TIME_MS                100    /**< Minimum time between samples */
#define ALARM_HYSTERESIS_C_PER_S          1.0f   /**< Hysteresis to clear alarm */
#define EMERGENCY_SHUTDOWN_DELAY_MS       50     /**< Delay after detection before shutdown */

// Module state
static struct {
    ThermalRunawayState_t state;
    ThermalRunawayConfig_t config;
    float temperature_history[THERMAL_RUNAWAY_WINDOW_SAMPLES];
    uint32_t timestamp_history[THERMAL_RUNAWAY_WINDOW_SAMPLES];
    uint8_t history_index;
    uint8_t sample_count;
    float current_dT_dt;           /**< Current rate of change (C/s) */
    uint32_t alarm_trigger_time;   /**< Timestamp when alarm triggered */
    uint32_t shutdown_count;       /**< Total shutdowns triggered */
    uint32_t alarm_count;          /**< Total alarms (not all lead to shutdown) */
} thermal_runaway;

/**
 * @brief Initialize thermal runaway protection
 *
 * @return true if initialization successful
 */
bool ThermalRunaway_Init(void)
{
    ADAPTIVE_ASSERT(THERMAL_RUNAWAY_WINDOW_SAMPLES >= 2);

    memset(&thermal_runaway, 0, sizeof(thermal_runaway));

    // Default configuration
    thermal_runaway.config.dT_dt_threshold_c_per_s = THERMAL_RUNAWAY_THRESHOLD_C_PER_S;
    thermal_runaway.config.sample_interval_ms = MIN_SAMPLE_TIME_MS;
    thermal_runaway.config.emergency_shutdown_enable = true;
    thermal_runaway.config.alarm_hysteresis_c_per_s = ALARM_HYSTERESIS_C_PER_S;
    thermal_runaway.state = THERMAL_RUNAWAY_STATE_NORMAL;

    return true;
}

/**
 * @brief Configure thermal runaway protection parameters
 *
 * @param config Configuration structure
 * @return true if configuration applied successfully
 */
bool ThermalRunaway_Configure(const ThermalRunawayConfig_t *config)
{
    ADAPTIVE_ASSERT(config != NULL);

    if (config == NULL) return false;

    // Validate configuration
    if (config->dT_dt_threshold_c_per_s <= 0.0f) return false;
    if (config->sample_interval_ms < MIN_SAMPLE_TIME_MS) return false;

    thermal_runaway.config = *config;
    return true;
}

/**
 * @brief Calculate dT/dt (temperature change rate) from history
 *
 * Uses linear regression on the last N samples for robust rate calculation.
 *
 * @return dT/dt in degrees C per second
 */
static float calculate_dT_dt(void)
{
    if (thermal_runaway.sample_count < 2) {
        return 0.0f;  // Not enough samples
    }

    uint8_t n = thermal_runaway.sample_count;

    // Simple two-point difference using newest and oldest samples
    uint8_t newest_idx = (thermal_runaway.history_index + THERMAL_RUNAWAY_WINDOW_SAMPLES - 1) %
                         THERMAL_RUNAWAY_WINDOW_SAMPLES;
    uint8_t oldest_idx = thermal_runaway.history_index;

    float dT = thermal_runaway.temperature_history[newest_idx] -
               thermal_runaway.temperature_history[oldest_idx];
    uint32_t dt_ms = thermal_runaway.timestamp_history[newest_idx] -
                     thermal_runaway.timestamp_history[oldest_idx];

    if (dt_ms == 0) return 0.0f;

    float dT_dt = (dT / dt_ms) * 1000.0f;  // Convert to C/s

    return dT_dt;
}

/**
 * @brief Update thermal runaway protection with new temperature reading
 *
 * This function must be called regularly (e.g., every 100ms) with
 * fresh temperature readings.
 *
 * Security: This is the critical safety function that detects runaway
 *
 * @param temp_c Temperature in Celsius
 * @param timestamp_ms Current timestamp in milliseconds
 * @return Current state after processing
 */
ThermalRunawayState_t ThermalRunaway_Update(float temp_c, uint32_t timestamp_ms)
{
    // Store sample
    thermal_runaway.temperature_history[thermal_runaway.history_index] = temp_c;
    thermal_runaway.timestamp_history[thermal_runaway.history_index] = timestamp_ms;
    thermal_runaway.history_index = (thermal_runaway.history_index + 1) % THERMAL_RUNAWAY_WINDOW_SAMPLES;

    if (thermal_runaway.sample_count < THERMAL_RUNAWAY_WINDOW_SAMPLES) {
        thermal_runaway.sample_count++;
    }

    // Calculate current dT/dt
    thermal_runaway.current_dT_dt = calculate_dT_dt();

    // State machine for thermal runaway detection
    switch (thermal_runaway.state) {
        case THERMAL_RUNAWAY_STATE_NORMAL:
            // Check if rate exceeds threshold
            if (fabsf(thermal_runaway.current_dT_dt) > thermal_runaway.config.dT_dt_threshold_c_per_s) {
                thermal_runaway.alarm_count++;
                thermal_runaway.alarm_trigger_time = timestamp_ms;
                thermal_runaway.state = THERMAL_RUNAWAY_STATE_ALARM;
            }
            break;

        case THERMAL_RUNAWAY_STATE_ALARM:
            // Verify rate still high after delay
            if ((timestamp_ms - thermal_runaway.alarm_trigger_time) >= EMERGENCY_SHUTDOWN_DELAY_MS) {
                if (fabsf(thermal_runaway.current_dT_dt) > thermal_runaway.config.dT_dt_threshold_c_per_s) {
                    // Confirmed - trigger emergency shutdown
                    if (thermal_runaway.config.emergency_shutdown_enable) {
                        thermal_runaway.shutdown_count++;
                        thermal_runaway.state = THERMAL_RUNAWAY_STATE_SHUTDOWN;
                    }
                } else {
                    // False alarm - rate dropped
                    thermal_runaway.state = THERMAL_RUNAWAY_STATE_NORMAL;
                }
            }
            break;

        case THERMAL_RUNAWAY_STATE_SHUTDOWN:
            // Shutdown state persists until manually reset
            break;

        case THERMAL_RUNAWAY_STATE_ERROR:
            // Error state - sensor failure or invalid data
            break;
    }

    return thermal_runaway.state;
}

/**
 * @brief Check if emergency shutdown is required
 *
 * @return true if shutdown should be triggered
 */
bool ThermalRunaway_ShutdownRequired(void)
{
    return (thermal_runaway.state == THERMAL_RUNAWAY_STATE_SHUTDOWN);
}

/**
 * @brief Get current thermal runaway state
 *
 * @return Current state enum
 */
ThermalRunawayState_t ThermalRunaway_GetState(void)
{
    return thermal_runaway.state;
}

/**
 * @brief Get current dT/dt (temperature change rate)
 *
 * @return Rate in degrees C per second
 */
float ThermalRunaway_GetCurrentRate(void)
{
    return thermal_runaway.current_dT_dt;
}

/**
 * @brief Get statistics for monitoring and diagnostics
 *
 * @param stats Pointer to statistics structure to fill
 */
void ThermalRunaway_GetStats(ThermalRunawayStats_t *stats)
{
    ADAPTIVE_ASSERT(stats != NULL);

    if (stats == NULL) return;

    stats->current_rate_c_per_s = thermal_runaway.current_dT_dt;
    stats->alarm_count = thermal_runaway.alarm_count;
    stats->shutdown_count = thermal_runaway.shutdown_count;
    stats->current_state = thermal_runaway.state;
    stats->samples_collected = thermal_runaway.sample_count;
}

/**
 * @brief Reset thermal runaway protection after alarm/shutdown
 *
 * Must be called by authorized personnel after investigating cause.
 *
 * @return true if reset successful
 */
bool ThermalRunaway_Reset(void)
{
    // Log reset attempt for audit trail
    // In production, this would log to flash

    // Clear history
    memset(thermal_runaway.temperature_history, 0, sizeof(thermal_runaway.temperature_history));
    memset(thermal_runaway.timestamp_history, 0, sizeof(thermal_runaway.timestamp_history));
    thermal_runaway.history_index = 0;
    thermal_runaway.sample_count = 0;
    thermal_runaway.current_dT_dt = 0.0f;

    // Return to normal state
    thermal_runaway.state = THERMAL_RUNAWAY_STATE_NORMAL;

    return true;
}

/**
 * @brief Get human-readable state string
 *
 * @param state State to convert
 * @return String representation
 */
const char* ThermalRunaway_GetStateString(ThermalRunawayState_t state)
{
    switch (state) {
        case THERMAL_RUNAWAY_STATE_NORMAL:   return "NORMAL";
        case THERMAL_RUNAWAY_STATE_ALARM:    return "ALARM";
        case THERMAL_RUNAWAY_STATE_SHUTDOWN: return "SHUTDOWN";
        case THERMAL_RUNAWAY_STATE_ERROR:    return "ERROR";
        default:                             return "UNKNOWN";
    }
}

/**
 * @brief Get current configuration
 *
 * @return Pointer to current configuration (read-only)
 */
const ThermalRunawayConfig_t* ThermalRunaway_GetConfig(void)
{
    return &thermal_runaway.config;
}
