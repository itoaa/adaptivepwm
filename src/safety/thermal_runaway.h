/**
 * @file thermal_runaway.h
 * @brief Thermal Runaway Protection Header
 *
 * Security Task: SEC-009 / ADP-005
 * CVSS: 9.1 - Critical
 *
 * Implements dT/dt > 5°C/s detection with emergency shutdown.
 * Thermal runaway can cause fires, equipment damage, or injury.
 *
 * Framework: CISSP Domain 3/8, NIST CSF PR.IP-1, ISO 27001 8.1
 */

#ifndef THERMAL_RUNAWAY_H
#define THERMAL_RUNAWAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of samples in temperature history window */
#define THERMAL_RUNAWAY_WINDOW_SAMPLES 10

/**
 * @brief Thermal runaway detection states
 *
 * State machine:
 * NORMAL -> ALARM (dT/dt > threshold)
 * ALARM -> SHUTDOWN (confirmed after delay)
 * ALARM -> NORMAL (false alarm)
 * SHUTDOWN (persistent until reset)
 */
typedef enum {
    THERMAL_RUNAWAY_STATE_NORMAL = 0,
    THERMAL_RUNAWAY_STATE_ALARM,
    THERMAL_RUNAWAY_STATE_SHUTDOWN,
    THERMAL_RUNAWAY_STATE_ERROR
} ThermalRunawayState_t;

/**
 * @brief Configuration for thermal runaway protection
 *
 * All parameters must be positive and validated.
 */
typedef struct {
    float dT_dt_threshold_c_per_s;    /**< dT/dt threshold for alarm (default: 5.0) */
    uint32_t sample_interval_ms;      /**< Expected sample interval in ms */
    bool emergency_shutdown_enable;   /**< Enable emergency shutdown (default: true) */
    float alarm_hysteresis_c_per_s;   /**< Hysteresis to clear alarm (default: 1.0) */
} ThermalRunawayConfig_t;

/**
 * @brief Statistics for diagnostics and monitoring
 */
typedef struct {
    float current_rate_c_per_s;       /**< Current dT/dt calculation */
    uint32_t alarm_count;             /**< Total alarms triggered */
    uint32_t shutdown_count;          /**< Total shutdowns triggered */
    ThermalRunawayState_t current_state;
    uint8_t samples_collected;        /**< Current sample buffer fill */
} ThermalRunawayStats_t;

/**
 * @brief Initialize thermal runaway protection
 *
 * Must be called before any other thermal runaway functions.
 *
 * @return true if initialization successful
 */
bool ThermalRunaway_Init(void);

/**
 * @brief Configure thermal runaway protection parameters
 *
 * @param config Configuration structure with valid parameters
 * @return true if configuration applied successfully
 */
bool ThermalRunaway_Configure(const ThermalRunawayConfig_t *config);

/**
 * @brief Update thermal runaway protection with new temperature reading
 *
 * Call this function regularly (typically every 100ms) with fresh
 * temperature readings to calculate dT/dt and detect thermal runaway.
 *
 * Security: This is the critical safety function.
 *
 * @param temp_c Temperature in Celsius
 * @param timestamp_ms Current timestamp in milliseconds
 * @return Current state after processing
 */
ThermalRunawayState_t ThermalRunaway_Update(float temp_c, uint32_t timestamp_ms);

/**
 * @brief Check if emergency shutdown is required
 *
 * @return true if shutdown should be triggered
 */
bool ThermalRunaway_ShutdownRequired(void);

/**
 * @brief Get current thermal runaway state
 *
 * @return Current state enum
 */
ThermalRunawayState_t ThermalRunaway_GetState(void);

/**
 * @brief Get current dT/dt (temperature change rate)
 *
 * @return Rate in degrees C per second
 */
float ThermalRunaway_GetCurrentRate(void);

/**
 * @brief Get statistics for monitoring and diagnostics
 *
 * @param stats Pointer to statistics structure to fill
 */
void ThermalRunaway_GetStats(ThermalRunawayStats_t *stats);

/**
 * @brief Reset thermal runaway protection after alarm/shutdown
 *
 * MUST be called by authorized personnel after investigating cause.
 * Logs reset event for audit trail.
 *
 * @return true if reset successful
 */
bool ThermalRunaway_Reset(void);

/**
 * @brief Get human-readable state string
 *
 * @param state State to convert
 * @return String representation
 */
const char* ThermalRunaway_GetStateString(ThermalRunawayState_t state);

/**
 * @brief Get current configuration
 *
 * @return Pointer to current configuration (read-only)
 */
const ThermalRunawayConfig_t* ThermalRunaway_GetConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMAL_RUNAWAY_H */
