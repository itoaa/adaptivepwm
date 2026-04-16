/**
 * @file enhanced_safety.h
 * @brief Enhanced Safety System with Fault Recovery and Graceful Degradation
 * 
 * Security Task: PWM-ARCH-004
 * 
 * Implements comprehensive safety system with:
 * - Automatic fault recovery for recoverable faults
 * - Graceful degradation of functionality on error
 * - Multi-level watchdog strategy
 * - CRC validation of critical data
 * - Diagnostic mode for detailed error reporting
 *
 * Framework: CISSP Domain 3/7/8, NIST CSF PR.IP-1/DE.AE, IEC 61508
 */

#ifndef ENHANCED_SAFETY_H
#define ENHANCED_SAFETY_H

#include <stdint.h>
#include <stdbool.h>
#include "fault_history.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safety system states
 */
typedef enum {
    SAFETY_STATE_INIT = 0,        // Initializing
    SAFETY_STATE_NORMAL,          // Normal operation
    SAFETY_STATE_DEGRADED_PWM,    // PWM limited due to fault
    SAFETY_STATE_DEGRADED_ADC,    // ADC reduced functionality
    SAFETY_STATE_RECOVERY,        // Attempting recovery
    SAFETY_STATE_SAFE_STOP,       // Safe stop initiated
    SAFETY_STATE_EMERGENCY,       // Emergency stop (critical fault)
    SAFETY_STATE_ERROR            // Unrecoverable error
} SafetyState_t;

/**
 * @brief Degradation levels
 */
typedef enum {
    DEGRADATION_NONE = 0,         // No degradation
    DEGRADATION_LIGHT,          // Minor performance reduction
    DEGRADATION_MODERATE,       // Significant reduction
    DEGRADATION_SEVERE,         // Minimal functionality
    DEGRADATION_CRITICAL        // Safe stop required
} DegradationLevel_t;

/**
 * @brief Recovery status
 */
typedef enum {
    RECOVERY_IDLE = 0,            // No recovery in progress
    RECOVERY_ATTEMPT_1,         // First recovery attempt
    RECOVERY_ATTEMPT_2,         // Second recovery attempt
    RECOVERY_ATTEMPT_3,         // Third recovery attempt
    RECOVERY_BACKOFF,           // Backing off before retry
    RECOVERY_SUCCESS,           // Recovery successful
    RECOVERY_FAILED             // Recovery failed
} RecoveryStatus_t;

/**
 * @brief Module watchdog types
 */
typedef enum {
    WDG_MODULE_MAIN = 0,          // Main system watchdog
    WDG_MODULE_PWM,               // PWM subsystem watchdog
    WDG_MODULE_ADC,               // ADC subsystem watchdog
    WDG_MODULE_COMM,              // Communication watchdog
    WDG_MODULE_SAFETY,            // Safety system watchdog
    WDG_MODULE_COUNT
} WatchdogModule_t;

/**
 * @brief Watchdog configuration
 */
typedef struct {
    uint32_t timeout_ms;          // Timeout period
    uint32_t warning_ms;          // Warning threshold (80% of timeout)
    bool auto_recovery;             // Enable auto-recovery on timeout
    bool panic_on_timeout;        // Enter panic mode on timeout
} WatchdogConfig_t;

/**
 * @brief Module health status
 */
typedef struct {
    bool healthy;                 // Module is healthy
    uint32_t last_checkin;        // Last watchdog checkin timestamp
    uint32_t timeout_count;       // Number of timeouts
    uint32_t recovery_count;      // Number of recoveries
    DegradationLevel_t degradation; // Current degradation level
} ModuleHealth_t;

/**
 * @brief Critical data with CRC protection
 */
typedef struct __attribute__((packed)) {
    float max_duty_cycle;         // Maximum allowed duty cycle
    float max_current;            // Maximum current limit
    float max_temp;               // Maximum temperature
    uint32_t safety_flags;        // Safety feature flags
    uint16_t config_version;      // Configuration version
    uint16_t crc;                 // CRC16 checksum
} CriticalSafetyData_t;

/**
 * @brief Safety configuration
 */
typedef struct {
    bool auto_recovery_enabled;       // Enable automatic recovery
    bool graceful_degradation_enabled; // Enable graceful degradation
    bool watchdog_enabled;            // Enable module watchdogs
    bool crc_validation_enabled;      // Enable CRC validation
    uint32_t recovery_backoff_ms;     // Backoff time between retries
    uint8_t max_recovery_attempts;    // Max recovery attempts
    float pwm_degradation_duty;      // Reduced duty cycle in degradation
    uint32_t diagnostic_mode_timeout; // Auto-disable diagnostic mode (ms)
} SafetyConfig_t;

/**
 * @brief Safety statistics
 */
typedef struct {
    uint32_t recovery_attempts;       // Total recovery attempts
    uint32_t recovery_successes;        // Successful recoveries
    uint32_t recovery_failures;         // Failed recoveries
    uint32_t degradations_triggered;    // Times degraded mode entered
    uint32_t emergency_stops;             // Emergency stops triggered
    uint32_t crc_failures;                // CRC validation failures
    uint32_t watchdog_timeouts;           // Module watchdog timeouts
    uint32_t system_uptime_ms;            // System uptime
} SafetyStatistics_t;

/**
 * @brief Safety manager
 */
typedef struct {
    SafetyState_t current_state;
    SafetyState_t previous_state;
    DegradationLevel_t degradation;
    RecoveryStatus_t recovery_status;
    bool diagnostic_mode;
    uint32_t diagnostic_mode_start;
    uint32_t state_entry_time;
    CriticalSafetyData_t critical_data;
    SafetyConfig_t config;
    SafetyStatistics_t stats;
    ModuleHealth_t module_health[WDG_MODULE_COUNT];
    bool initialized;
} EnhancedSafetyManager_t;

// Configuration defaults
#define SAFETY_DEFAULT_RECOVERY_BACKOFF_MS    5000
#define SAFETY_DEFAULT_MAX_RETRIES            3
#define SAFETY_DEFAULT_PWM_DEGRADATION_DUTY   0.5f  // 50% duty in degradation
#define SAFETY_DEFAULT_DIAGNOSTIC_TIMEOUT     300000  // 5 minutes

#define SAFETY_CRITICAL_DATA_MAGIC            0x53AF  // "SA"fety
#define SAFETY_CONFIG_VERSION                 0x0100

// Watchdog timeouts
#define WDG_TIMEOUT_MAIN_MS                   1000
#define WDG_TIMEOUT_PWM_MS                    500
#define WDG_TIMEOUT_ADC_MS                    250
#define WDG_TIMEOUT_COMM_MS                   2000
#define WDG_TIMEOUT_SAFETY_MS                 500

/**
 * @brief Initialize enhanced safety system
 * 
 * Must be called before any other safety functions.
 *
 * @param manager Safety manager
 * @return true if initialization successful
 */
bool EnhancedSafety_Init(EnhancedSafetyManager_t* manager);

/**
 * @brief Deinitialize safety system
 * 
 * @param manager Safety manager
 * @return true if successful
 */
bool EnhancedSafety_Deinit(EnhancedSafetyManager_t* manager);

/**
 * @brief Process safety system (call periodically)
 * 
 * Handles state transitions, recovery attempts, and watchdog monitoring.
 * Should be called at least every 100ms.
 *
 * @param manager Safety manager
 * @return Current safety state
 */
SafetyState_t EnhancedSafety_Process(EnhancedSafetyManager_t* manager);

/**
 * @brief Report a fault to safety system
 * 
 * Triggers appropriate recovery or degradation based on fault type.
 *
 * @param manager Safety manager
 * @param fault_type Type of fault
 * @param severity Fault severity
 * @param error_code Error code
 * @param context Additional context
 * @return true if fault handled
 */
bool EnhancedSafety_ReportFault(EnhancedSafetyManager_t* manager, 
                                 FaultType_t fault_type,
                                 FaultSeverity_t severity,
                                 uint16_t error_code,
                                 uint32_t context);

/**
 * @brief Enter diagnostic mode
 * 
 * Enables extended logging and diagnostics.
 * Auto-disables after timeout for safety.
 *
 * @param manager Safety manager
 * @return true if entered diagnostic mode
 */
bool EnhancedSafety_EnterDiagnosticMode(EnhancedSafetyManager_t* manager);

/**
 * @brief Exit diagnostic mode
 * 
 * @param manager Safety manager
 */
void EnhancedSafety_ExitDiagnosticMode(EnhancedSafetyManager_t* manager);

/**
 * @brief Check if in diagnostic mode
 * 
 * @param manager Safety manager
 * @return true if diagnostic mode active
 */
bool EnhancedSafety_IsDiagnosticMode(const EnhancedSafetyManager_t* manager);

/**
 * @brief Get current safety state
 * 
 * @param manager Safety manager
 * @return Current state
 */
SafetyState_t EnhancedSafety_GetState(const EnhancedSafetyManager_t* manager);

/**
 * @brief Get current degradation level
 * 
 * @param manager Safety manager
 * @return Degradation level
 */
DegradationLevel_t EnhancedSafety_GetDegradationLevel(const EnhancedSafetyManager_t* manager);

/**
 * @brief Get recovery status
 * 
 * @param manager Safety manager
 * @return Recovery status
 */
RecoveryStatus_t EnhancedSafety_GetRecoveryStatus(const EnhancedSafetyManager_t* manager);

/**
 * @brief Check if auto-recovery is in progress
 * 
 * @param manager Safety manager
 * @return true if recovering
 */
bool EnhancedSafety_IsRecovering(const EnhancedSafetyManager_t* manager);

/**
 * @brief Request manual recovery attempt
 * 
 * @param manager Safety manager
 * @return true if recovery initiated
 */
bool EnhancedSafety_RequestRecovery(EnhancedSafetyManager_t* manager);

/**
 * @brief Trigger emergency stop
 * 
 * Immediately stops all operation and enters safe state.
 *
 * @param manager Safety manager
 * @param reason Reason for emergency stop
 */
void EnhancedSafety_EmergencyStop(EnhancedSafetyManager_t* manager, const char* reason);

/**
 * @brief Check if emergency stop is active
 * 
 * @param manager Safety manager
 * @return true if emergency stop active
 */
bool EnhancedSafety_IsEmergencyStop(const EnhancedSafetyManager_t* manager);

/**
 * @brief Clear emergency stop (after manual intervention)
 * 
 * @param manager Safety manager
 * @return true if cleared
 */
bool EnhancedSafety_ClearEmergencyStop(EnhancedSafetyManager_t* manager);

/**
 * @brief Get effective duty cycle limit
 * 
 * Returns reduced limit when in degraded mode.
 *
 * @param manager Safety manager
 * @param requested_duty Requested duty cycle
 * @return Effective duty cycle
 */
float EnhancedSafety_GetEffectiveDutyLimit(const EnhancedSafetyManager_t* manager,
                                            float requested_duty);

/**
 * @brief Get effective current limit
 * 
 * @param manager Safety manager
 * @return Effective current limit
 */
float EnhancedSafety_GetEffectiveCurrentLimit(const EnhancedSafetyManager_t* manager);

/**
 * @brief Watchdog checkin for modules
 * 
 * Call periodically from each module to prevent timeout.
 *
 * @param manager Safety manager
 * @param module Module identifier
 * @return true if checkin successful
 */
bool EnhancedSafety_WatchdogCheckin(EnhancedSafetyManager_t* manager, 
                                      WatchdogModule_t module);

/**
 * @brief Set module health
 * 
 * @param manager Safety manager
 * @param module Module identifier
 * @param healthy true if healthy
 * @param degradation Degradation level
 * @return true if updated
 */
bool EnhancedSafety_SetModuleHealth(EnhancedSafetyManager_t* manager,
                                     WatchdogModule_t module,
                                     bool healthy,
                                     DegradationLevel_t degradation);

/**
 * @brief Get module health
 * 
 * @param manager Safety manager
 * @param module Module identifier
 * @return Health status
 */
ModuleHealth_t EnhancedSafety_GetModuleHealth(const EnhancedSafetyManager_t* manager,
                                                WatchdogModule_t module);

/**
 * @brief Validate critical safety data
 * 
 * Verifies CRC of critical configuration.
 *
 * @param manager Safety manager
 * @return true if data valid
 */
bool EnhancedSafety_ValidateCriticalData(const EnhancedSafetyManager_t* manager);

/**
 * @brief Update critical safety data
 * 
 * Updates and recalculates CRC.
 *
 * @param manager Safety manager
 * @param data New critical data
 * @return true if updated
 */
bool EnhancedSafety_UpdateCriticalData(EnhancedSafetyManager_t* manager,
                                      const CriticalSafetyData_t* data);

/**
 * @brief Calculate CRC16 for critical data
 * 
 * @param data Critical data
 * @return CRC value
 */
uint16_t EnhancedSafety_CalculateCRC16(const CriticalSafetyData_t* data);

/**
 * @brief Get safety statistics
 * 
 * @param manager Safety manager
 * @param stats Output buffer
 */
void EnhancedSafety_GetStatistics(const EnhancedSafetyManager_t* manager,
                                   SafetyStatistics_t* stats);

/**
 * @brief Get safety configuration
 * 
 * @param manager Safety manager
 * @return Configuration pointer (read-only)
 */
const SafetyConfig_t* EnhancedSafety_GetConfig(const EnhancedSafetyManager_t* manager);

/**
 * @brief Update safety configuration
 * 
 * @param manager Safety manager
 * @param config New configuration
 * @return true if updated
 */
bool EnhancedSafety_SetConfig(EnhancedSafetyManager_t* manager,
                               const SafetyConfig_t* config);

/**
 * @brief Get human-readable state string
 * 
 * @param state Safety state
 * @return String representation
 */
const char* EnhancedSafety_GetStateString(SafetyState_t state);

/**
 * @brief Get human-readable degradation string
 * 
 * @param level Degradation level
 * @return String representation
 */
const char* EnhancedSafety_GetDegradationString(DegradationLevel_t level);

/**
 * @brief Get human-readable recovery status string
 * 
 * @param status Recovery status
 * @return String representation
 */
const char* EnhancedSafety_GetRecoveryStatusString(RecoveryStatus_t status);

/**
 * @brief Get human-readable module name
 * 
 * @param module Module identifier
 * @return String representation
 */
const char* EnhancedSafety_GetModuleString(WatchdogModule_t module);

/**
 * @brief Format safety status as string
 * 
 * @param manager Safety manager
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
uint16_t EnhancedSafety_FormatStatus(const EnhancedSafetyManager_t* manager,
                                      char* buffer, uint16_t size);

/**
 * @brief Run self-test
 * 
 * Performs internal diagnostics on safety system.
 *
 * @param manager Safety manager
 * @return true if self-test passed
 */
bool EnhancedSafety_SelfTest(EnhancedSafetyManager_t* manager);

#ifdef __cplusplus
}
#endif

#endif /* ENHANCED_SAFETY_H */
