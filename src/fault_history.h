/**
 * @file fault_history.h
 * @brief Fault History Logging System with Persistent Flash Storage
 * 
 * Security Task: PWM-ARCH-004
 * 
 * Implements circular buffer fault logging to internal Flash memory.
 * Logs survive system resets for post-mortem analysis.
 * Supports predictive maintenance through pattern analysis.
 * 
 * FLASH WEAR LEVELING (PWM-ARCH-004):
 * - Circular buffer with 256 entries
 * - Even distribution of writes across sector
 * - Sector erase only when full (128KB sector)
 * - STM32F401 flash endurance: 10,000 cycles per sector
 * - At 1 entry/minute: ~27 hours before wrap
 * - Wear statistics tracking for maintenance planning
 *
 * Framework: CISSP Domain 7, NIST CSF DE.AE, IEC 61508
 */

#ifndef FAULT_HISTORY_H
#define FAULT_HISTORY_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fault types for classification and analysis
 */
typedef enum {
    FAULT_TYPE_NONE = 0,
    FAULT_TYPE_OVER_VOLTAGE,
    FAULT_TYPE_UNDER_VOLTAGE,
    FAULT_TYPE_OVER_CURRENT,
    FAULT_TYPE_OVER_TEMP,
    FAULT_TYPE_THERMAL_RUNAWAY,
    FAULT_TYPE_PWM_FAULT,
    FAULT_TYPE_ADC_FAILURE,
    FAULT_TYPE_WATCHDOG_TIMEOUT,
    FAULT_TYPE_COMMUNICATION,
    FAULT_TYPE_CRC_ERROR,
    FAULT_TYPE_CONFIG_CORRUPT,
    FAULT_TYPE_HARDWARE_FAULT,
    FAULT_TYPE_SOFTWARE_FAULT,
    FAULT_TYPE_RECOVERY_SUCCESS,
    FAULT_TYPE_RECOVERY_FAILED,
    FAULT_TYPE_COUNT
} FaultType_t;

/**
 * @brief Fault severity levels
 */
typedef enum {
    FAULT_SEVERITY_INFO = 0,      // Informational only
    FAULT_SEVERITY_WARNING,       // Warning, operation continues
    FAULT_SEVERITY_ERROR,         // Error, may recover
    FAULT_SEVERITY_CRITICAL,      // Critical, safe state required
    FAULT_SEVERITY_FATAL          // Fatal, system halted
} FaultSeverity_t;

/**
 * @brief Recovery action types
 */
typedef enum {
    RECOVERY_ACTION_NONE = 0,
    RECOVERY_ACTION_RETRY,        // Simple retry
    RECOVERY_ACTION_RESET,        // Module reset
    RECOVERY_ACTION_DEGRADE,        // Graceful degradation
    RECOVERY_ACTION_STOP,           // Stop operation
    RECOVERY_ACTION_RESTART,        // Full restart
    RECOVERY_ACTION_MANUAL          // Require manual intervention
} RecoveryAction_t;

/**
 * @brief Recovery result
 */
typedef enum {
    RECOVERY_RESULT_PENDING = 0,
    RECOVERY_RESULT_SUCCESS,
    RECOVERY_RESULT_PARTIAL,
    RECOVERY_RESULT_FAILED,
    RECOVERY_RESULT_TIMEOUT
} RecoveryResult_t;

/**
 * @brief Single fault entry in history
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;        // System uptime when fault occurred
    uint32_t rtc_timestamp;       // Real-time clock if available
    FaultType_t fault_type;       // Type of fault
    FaultSeverity_t severity;     // Severity level
    uint16_t error_code;          // Original error code
    uint32_t context_data;        // Context-specific data
    RecoveryAction_t recovery_action; // Attempted recovery
    RecoveryResult_t recovery_result; // Result of recovery
    uint8_t retry_count;          // Number of retry attempts
    uint16_t crc;                 // CRC for data integrity
} FaultEntry_t;

/**
 * @brief Fault statistics for predictive maintenance
 */
typedef struct {
    uint32_t total_faults;                    // Total faults logged
    uint32_t faults_by_type[FAULT_TYPE_COUNT]; // Faults per type
    uint32_t total_recoveries;                // Successful recoveries
    uint32_t failed_recoveries;               // Failed recovery attempts
    uint32_t system_resets;                   // Number of system resets
    uint32_t last_fault_timestamp;            // Time of last fault
    uint32_t fault_rate_1h;                   // Faults per hour (sliding window)
    uint32_t fault_rate_24h;                  // Faults per 24h (sliding window)
    uint32_t uptime_ms_at_last_fault;         // System uptime at last fault
} FaultStatistics_t;

/**
 * @brief Flash wear leveling statistics (PWM-ARCH-004)
 * 
 * Tracks flash endurance metrics for maintenance planning
 * and wear distribution analysis.
 */
typedef struct {
    uint32_t total_writes;        // Total write operations to flash
    uint32_t sector_erases;       // Number of sector erase cycles
    uint32_t oldest_entry_age;    // Age of oldest entry in writes
    uint32_t current_index;       // Current write position
    uint32_t oldest_index;        // Oldest valid entry index
    uint32_t wrap_count;          // Number of buffer wraps
    float average_wear;           // Average writes per entry location (0-100%)
    float max_wear;               // Maximum wear on any location (0-100%)
    uint32_t estimated_life_pct;  // Estimated remaining flash life (0-100%)
} FlashWearStats_t;

/**
 * @brief Predictive maintenance indicators
 */
typedef struct {
    bool maintenance_recommended;     // Maintenance recommended
    uint32_t days_until_maintenance;  // Estimated days until maintenance needed
    float health_score;               // Overall system health (0.0 - 100.0)
    float degradation_rate;           // Health degradation rate per day
    uint32_t trend_direction;         // 0=stable, 1=improving, 2=degrading
    const char* primary_concern;      // Primary concern if maintenance needed
} MaintenancePrediction_t;

/**
 * @brief Fault history manager
 */
typedef struct {
    bool initialized;
    uint32_t write_index;             // Next write position in flash
    uint32_t entry_count;             // Total entries in log
    uint32_t wrap_count;              // Number of times buffer wrapped
    uint32_t last_fault_timestamp;    // Timestamp of last fault
    uint32_t oldest_index;            // Oldest valid entry index
    uint8_t recovery_attempts;        // Current recovery attempt counter
    bool diagnostic_mode;             // Extended logging enabled
    FaultStatistics_t statistics;   // Running statistics
    FlashWearStats_t wear_stats;    // Flash wear statistics (PWM-ARCH-004)
} FaultHistory_t;

// Configuration
#define FAULT_HISTORY_VERSION       0x0100  // Version 1.0
#define FAULT_HISTORY_MAGIC         0xFA1B  // Magic number for validation
#define FAULT_HISTORY_MAX_ENTRIES   256     // Maximum fault entries in flash
#define FAULT_HISTORY_FLASH_SECTOR  FLASH_SECTOR_6  // Use sector 6 (before log sector)
#define FAULT_HISTORY_FLASH_ADDR    0x080C0000  // Sector 6 base address
#define FAULT_HISTORY_FLASH_SIZE    0x00020000  // 128KB sector

// STM32F401 flash endurance specification
#define FLASH_ENDURANCE_CYCLES      10000   // Minimum erase cycles per sector

// Recovery configuration
#define FAULT_MAX_RETRIES           3       // Max retry attempts
#define FAULT_RETRY_DELAY_MS        100     // Delay between retries
#define FAULT_RECOVERY_TIMEOUT_MS   5000    // Recovery timeout
#define FAULT_DEGRADATION_THRESHOLD 5       // Faults before degradation

// Predictive maintenance thresholds
#define MAINTENANCE_FAULT_THRESHOLD     10  // Faults per week triggers maintenance
#define MAINTENANCE_HEALTH_THRESHOLD    70.0f  // Health score below this triggers warning
#define MAINTENANCE_DEGRADATION_THRESHOLD 2.0f  // Degradation rate threshold

// Wear leveling thresholds
#define WEAR_LEVEL_CRITICAL_PCT     80.0f   // Critical wear level warning
#define WEAR_LEVEL_WARNING_PCT      60.0f   // Warning wear level
#define WEAR_LEVEL_SAFE_PCT         30.0f   // Safe wear level

/**
 * @brief Initialize fault history system
 * 
 * Loads existing fault log from flash if present.
 * Must be called before any other fault history functions.
 *
 * @return true if initialization successful
 */
bool FaultHistory_Init(void);

/**
 * @brief Deinitialize fault history system
 * 
 * Ensures all pending writes are flushed to flash.
 *
 * @return true if successful
 */
bool FaultHistory_Deinit(void);

/**
 * @brief Log a fault event
 * 
 * Records fault to persistent storage and updates statistics.
 * Automatically manages circular buffer wrapping with wear leveling.
 *
 * @param fault_type Type of fault
 * @param severity Fault severity
 * @param error_code Original error code
 * @param context Additional context data
 * @return true if logged successfully
 */
bool FaultHistory_Log(FaultType_t fault_type, FaultSeverity_t severity,
                      uint16_t error_code, uint32_t context);

/**
 * @brief Log a fault with recovery information
 * 
 * Records fault and associated recovery attempt.
 *
 * @param fault_type Type of fault
 * @param severity Fault severity
 * @param error_code Original error code
 * @param context Additional context data
 * @param recovery_action Recovery action attempted
 * @param recovery_result Result of recovery attempt
 * @return true if logged successfully
 */
bool FaultHistory_LogWithRecovery(FaultType_t fault_type, FaultSeverity_t severity,
                                  uint16_t error_code, uint32_t context,
                                  RecoveryAction_t recovery_action,
                                  RecoveryResult_t recovery_result);

/**
 * @brief Read a fault entry from history
 * 
 * @param index Entry index (0 = most recent)
 * @param entry Output buffer for fault entry
 * @return true if entry read successfully
 */
bool FaultHistory_Read(uint32_t index, FaultEntry_t* entry);

/**
 * @brief Get number of fault entries in history
 * 
 * @return Number of entries
 */
uint32_t FaultHistory_GetCount(void);

/**
 * @brief Clear fault history
 * 
 * Erases flash sector and resets statistics.
 * Should be used cautiously - destroys forensic data.
 *
 * @return true if cleared successfully
 */
bool FaultHistory_Clear(void);

/**
 * @brief Get fault statistics
 * 
 * @param stats Output buffer for statistics
 */
void FaultHistory_GetStatistics(FaultStatistics_t* stats);

/**
 * @brief Get flash wear statistics (PWM-ARCH-004)
 * 
 * Retrieves flash wear leveling statistics for maintenance planning.
 * Includes write counts, sector erases, and wear distribution.
 *
 * @param stats Output buffer for wear statistics
 */
void FaultHistory_GetWearStats(FlashWearStats_t* stats);

/**
 * @brief Format wear statistics as human-readable string
 * 
 * @param stats Wear statistics to format
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of bytes written
 */
uint16_t FaultHistory_FormatWearStats(const FlashWearStats_t* stats, 
                                        char* buffer, uint16_t size);

/**
 * @brief Set diagnostic mode
 * 
 * Enables extended logging for detailed diagnostics.
 * Increases flash wear - use only when needed.
 *
 * @param enable true to enable diagnostic mode
 */
void FaultHistory_SetDiagnosticMode(bool enable);

/**
 * @brief Check if diagnostic mode is enabled
 * 
 * @return true if diagnostic mode enabled
 */
bool FaultHistory_IsDiagnosticMode(void);

/**
 * @brief Get predictive maintenance prediction
 * 
 * Analyzes fault patterns to predict maintenance needs.
 *
 * @param prediction Output buffer for prediction
 */
void FaultHistory_GetMaintenancePrediction(MaintenancePrediction_t* prediction);

/**
 * @brief Get human-readable fault type string
 * 
 * @param fault_type Fault type
 * @return String representation
 */
const char* FaultHistory_GetTypeString(FaultType_t fault_type);

/**
 * @brief Get human-readable severity string
 * 
 * @param severity Severity level
 * @return String representation
 */
const char* FaultHistory_GetSeverityString(FaultSeverity_t severity);

/**
 * @brief Get human-readable recovery action string
 * 
 * @param action Recovery action
 * @return String representation
 */
const char* FaultHistory_GetRecoveryActionString(RecoveryAction_t action);

/**
 * @brief Get human-readable recovery result string
 * 
 * @param result Recovery result
 * @return String representation
 */
const char* FaultHistory_GetRecoveryResultString(RecoveryResult_t result);

/**
 * @brief Format fault entry as string
 * 
 * @param entry Fault entry to format
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of bytes written
 */
uint16_t FaultHistory_FormatEntry(const FaultEntry_t* entry, char* buffer, uint16_t size);

/**
 * @brief Get fault log as formatted text
 * 
 * @param buffer Output buffer
 * @param size Buffer size
 * @param max_entries Maximum entries to include
 * @return Number of bytes written
 */
uint16_t FaultHistory_GetLogText(char* buffer, uint16_t size, uint32_t max_entries);

/**
 * @brief Update fault type from legacy error code
 * 
 * Maps legacy error codes to fault types.
 *
 * @param error_code Legacy error code
 * @return Corresponding fault type
 */
FaultType_t FaultHistory_MapErrorCode(uint16_t error_code);

/**
 * @brief Get entries since timestamp
 * 
 * @param since_timestamp Only return entries after this time
 * @param entries Output array
 * @param max_entries Maximum entries to return
 * @return Number of entries returned
 */
uint32_t FaultHistory_GetEntriesSince(uint32_t since_timestamp, FaultEntry_t* entries, 
                                       uint32_t max_entries);

/**
 * @brief Get fault pattern analysis
 * 
 * Analyzes recent faults for patterns (burst detection, etc.)
 *
 * @param window_ms Time window to analyze
 * @param burst_detected Output - true if fault burst detected
 * @param fault_rate Output - faults per hour in window
 * @return true if analysis completed
 */
bool FaultHistory_AnalyzePattern(uint32_t window_ms, bool* burst_detected, 
                                  float* fault_rate);

/**
 * @brief Validate flash wear leveling integrity (PWM-ARCH-004)
 * 
 * Checks if wear leveling state is consistent and
 * entries are valid. Can be used for diagnostics.
 *
 * @param errors_out Output - number of errors found
 * @return true if validation passed, false if errors found
 */
bool FaultHistory_ValidateWearLeveling(uint32_t* errors_out);

/**
 * @brief Get wear level status string
 * 
 * @param wear_pct Wear percentage (0-100)
 * @return Human-readable status string
 */
const char* FaultHistory_GetWearStatusString(float wear_pct);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_HISTORY_H */
