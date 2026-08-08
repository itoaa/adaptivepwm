/**
 * @file enhanced_safety.c
 * @brief Enhanced Safety System Implementation
 * 
 * Security Task: PWM-ARCH-004
 * 
 * Implements comprehensive safety with automatic recovery,
 * graceful degradation, and multi-level watchdog strategy.
 *
 * Framework: CISSP Domain 3/7/8, NIST CSF PR.IP-1/DE.AE, IEC 61508
 */

#include "enhanced_safety.h"
#include "adaptive_assert.h"
#include "hal_pwm.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

// Module state
static struct {
    EnhancedSafetyManager_t* active_manager;
    uint32_t last_process_time;
    bool processing;
} safety_state = {0};

// Default critical safety data
static const CriticalSafetyData_t default_critical_data = {
    .max_duty_cycle = 0.95f,      // 95% max duty
    .max_current = 10.0f,         // 10A max current
    .max_temp = 85.0f,            // 85C max temperature
    .safety_flags = 0xFFFF,       // All safety features enabled
    .config_version = SAFETY_CONFIG_VERSION,
    .crc = 0
};

// Default safety configuration
static const SafetyConfig_t default_config = {
    .auto_recovery_enabled = true,
    .graceful_degradation_enabled = true,
    .watchdog_enabled = true,
    .crc_validation_enabled = true,
    .recovery_backoff_ms = SAFETY_DEFAULT_RECOVERY_BACKOFF_MS,
    .max_recovery_attempts = SAFETY_DEFAULT_MAX_RETRIES,
    .pwm_degradation_duty = SAFETY_DEFAULT_PWM_DEGRADATION_DUTY,
    .diagnostic_mode_timeout = SAFETY_DEFAULT_DIAGNOSTIC_TIMEOUT
};

// Default watchdog configuration
static const WatchdogConfig_t default_watchdog_config[WDG_MODULE_COUNT] = {
    [WDG_MODULE_MAIN]  = {WDG_TIMEOUT_MAIN_MS,  800,  true,  true},
    [WDG_MODULE_PWM]   = {WDG_TIMEOUT_PWM_MS,   400,  true,  true},
    [WDG_MODULE_ADC]   = {WDG_TIMEOUT_ADC_MS,   200,  true,  false},
    [WDG_MODULE_COMM]  = {WDG_TIMEOUT_COMM_MS,  1600, false, false},
    [WDG_MODULE_SAFETY] = {WDG_TIMEOUT_SAFETY_MS, 400,  true,  true}
};

// Recovery action mapping
static const struct {
    FaultType_t fault;
    RecoveryAction_t action;
    uint8_t max_attempts;
    SafetyState_t failure_state;
} recovery_map[] = {
    {FAULT_TYPE_OVER_VOLTAGE,     RECOVERY_ACTION_DEGRADE,    3, SAFETY_STATE_DEGRADED_PWM},
    {FAULT_TYPE_UNDER_VOLTAGE,    RECOVERY_ACTION_DEGRADE,    3, SAFETY_STATE_DEGRADED_PWM},
    {FAULT_TYPE_OVER_CURRENT,     RECOVERY_ACTION_STOP,       2, SAFETY_STATE_SAFE_STOP},
    {FAULT_TYPE_OVER_TEMP,        RECOVERY_ACTION_DEGRADE,    2, SAFETY_STATE_DEGRADED_PWM},
    {FAULT_TYPE_THERMAL_RUNAWAY,  RECOVERY_ACTION_STOP,       0, SAFETY_STATE_EMERGENCY},
    {FAULT_TYPE_PWM_FAULT,        RECOVERY_ACTION_RESET,      3, SAFETY_STATE_DEGRADED_PWM},
    {FAULT_TYPE_ADC_FAILURE,      RECOVERY_ACTION_RETRY,      2, SAFETY_STATE_DEGRADED_ADC},
    {FAULT_TYPE_WATCHDOG_TIMEOUT, RECOVERY_ACTION_RESTART,    1, SAFETY_STATE_EMERGENCY},
    {FAULT_TYPE_COMMUNICATION,    RECOVERY_ACTION_RETRY,      5, SAFETY_STATE_NORMAL},
    {FAULT_TYPE_CRC_ERROR,        RECOVERY_ACTION_RETRY,      3, SAFETY_STATE_SAFE_STOP},
    {FAULT_TYPE_CONFIG_CORRUPT,   RECOVERY_ACTION_MANUAL,     0, SAFETY_STATE_ERROR},
    {FAULT_TYPE_HARDWARE_FAULT,   RECOVERY_ACTION_MANUAL,     0, SAFETY_STATE_ERROR},
    {FAULT_TYPE_SOFTWARE_FAULT,   RECOVERY_ACTION_RESTART,    1, SAFETY_STATE_EMERGENCY},
    {FAULT_TYPE_NONE,             RECOVERY_ACTION_NONE,       0, SAFETY_STATE_NORMAL}
};

/**
 * @brief Calculate CRC16 for critical data
 * 
 * @param data Critical data
 * @return CRC value
 */
static uint16_t calculate_crc16(const CriticalSafetyData_t* data)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t crc = 0xFFFF;
    size_t len = sizeof(CriticalSafetyData_t) - sizeof(uint16_t);
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Get recovery mapping for fault type
 * 
 * @param fault Fault type
 * @return Recovery mapping pointer, or NULL if not found
 */
static const struct {
    FaultType_t fault;
    RecoveryAction_t action;
    uint8_t max_attempts;
    SafetyState_t failure_state;
}* get_recovery_mapping(FaultType_t fault)
{
    for (int i = 0; i < sizeof(recovery_map)/sizeof(recovery_map[0]); i++) {
        if (recovery_map[i].fault == fault) {
            return &recovery_map[i];
        }
    }
    return NULL;
}

/**
 * @brief Transition to new state
 * 
 * @param manager Safety manager
 * @param new_state New state
 */
static void transition_state(EnhancedSafetyManager_t* manager, SafetyState_t new_state)
{
    if (manager == NULL) return;
    
    manager->previous_state = manager->current_state;
    manager->current_state = new_state;
    manager->state_entry_time = HAL_GetTick();
    
    // Log state transition in diagnostic mode
    if (manager->diagnostic_mode) {
        // Would log here if FaultHistory is available
    }
}

/**
 * @brief Attempt fault recovery
 * 
 * @param manager Safety manager
 * @param fault Fault type
 * @return true if recovery should be attempted
 */
static bool attempt_recovery(EnhancedSafetyManager_t* manager, FaultType_t fault)
{
    if (!manager->config.auto_recovery_enabled) {
        return false;
    }
    
    const struct {
        FaultType_t fault;
        RecoveryAction_t action;
        uint8_t max_attempts;
        SafetyState_t failure_state;
    }* mapping = get_recovery_mapping(fault);
    
    if (mapping == NULL || mapping->action == RECOVERY_ACTION_NONE) {
        return false;
    }
    
    if (mapping->action == RECOVERY_ACTION_MANUAL) {
        // Manual intervention required
        return false;
    }
    
    // Check attempt count
    if (manager->stats.recovery_attempts >= manager->config.max_recovery_attempts) {
        return false;
    }
    
    return true;
}

/**
 * @brief Execute recovery action
 * 
 * @param manager Safety manager
 * @param action Recovery action
 * @return Recovery result
 */
static RecoveryResult_t execute_recovery(EnhancedSafetyManager_t* manager, 
                                          RecoveryAction_t action)
{
    (void)manager;
    
    RecoveryResult_t result = RECOVERY_RESULT_FAILED;
    
    switch (action) {
        case RECOVERY_ACTION_RETRY:
            // Simple retry - delay and continue
            HAL_Delay(100);
            result = RECOVERY_RESULT_SUCCESS;
            break;
            
        case RECOVERY_ACTION_RESET:
            // Reset affected module (simulated)
            HAL_Delay(500);
            result = RECOVERY_RESULT_SUCCESS;
            break;
            
        case RECOVERY_ACTION_DEGRADE:
            // Enter degraded mode
            if (manager->config.graceful_degradation_enabled) {
                manager->degradation = DEGRADATION_MODERATE;
                manager->stats.degradations_triggered++;
                result = RECOVERY_RESULT_SUCCESS;
            } else {
                result = RECOVERY_RESULT_FAILED;
            }
            break;
            
        case RECOVERY_ACTION_STOP:
            // Safe stop
            transition_state(manager, SAFETY_STATE_SAFE_STOP);
            result = RECOVERY_RESULT_SUCCESS;
            break;
            
        case RECOVERY_ACTION_RESTART:
            // Request system restart
            result = RECOVERY_RESULT_FAILED;  // Will be handled by main
            break;
            
        case RECOVERY_ACTION_MANUAL:
            result = RECOVERY_RESULT_FAILED;
            break;
            
        default:
            result = RECOVERY_RESULT_FAILED;
            break;
    }
    
    return result;
}

/**
 * @brief Process recovery state machine
 * 
 * @param manager Safety manager
 */
static void process_recovery(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) return;
    
    uint32_t now = HAL_GetTick();
    
    switch (manager->recovery_status) {
        case RECOVERY_IDLE:
            // Nothing to do
            break;
            
        case RECOVERY_ATTEMPT_1:
        case RECOVERY_ATTEMPT_2:
        case RECOVERY_ATTEMPT_3:
            // Recovery in progress - handled by fault reporting
            break;
            
        case RECOVERY_BACKOFF:
            // Wait for backoff period
            if (now - manager->state_entry_time >= manager->config.recovery_backoff_ms) {
                manager->recovery_status = RECOVERY_IDLE;
            }
            break;
            
        case RECOVERY_SUCCESS:
            // Recovery successful - return to normal
            manager->degradation = DEGRADATION_NONE;
            transition_state(manager, SAFETY_STATE_NORMAL);
            manager->recovery_status = RECOVERY_IDLE;
            break;
            
        case RECOVERY_FAILED:
            // Recovery failed - enter appropriate state
            transition_state(manager, SAFETY_STATE_ERROR);
            manager->recovery_status = RECOVERY_IDLE;
            break;
    }
}

/**
 * @brief Check module watchdogs
 * 
 * @param manager Safety manager
 */
static void check_watchdogs(EnhancedSafetyManager_t* manager)
{
    if (!manager->config.watchdog_enabled) {
        return;
    }
    
    uint32_t now = HAL_GetTick();
    
    for (int i = 0; i < WDG_MODULE_COUNT; i++) {
        ModuleHealth_t* health = &manager->module_health[i];
        const WatchdogConfig_t* config = &default_watchdog_config[i];
        
        if (!health->healthy) {
            continue;  // Already marked unhealthy
        }
        
        // Check for timeout
        if (now - health->last_checkin > config->timeout_ms) {
            health->timeout_count++;
            manager->stats.watchdog_timeouts++;
            
            if (config->panic_on_timeout) {
                // Trigger emergency stop
                EnhancedSafety_EmergencyStop(manager, "Watchdog timeout");
            } else {
                // Mark module unhealthy
                health->healthy = false;
                
                // Try auto-recovery
                if (config->auto_recovery) {
                    health->degradation = DEGRADATION_MODERATE;
                    health->recovery_count++;
                    manager->stats.recovery_attempts++;
                }
            }
        }
    }
}

/**
 * @brief Check diagnostic mode timeout
 * 
 * @param manager Safety manager
 */
static void check_diagnostic_timeout(EnhancedSafetyManager_t* manager)
{
    if (!manager->diagnostic_mode) {
        return;
    }
    
    uint32_t now = HAL_GetTick();
    if (now - manager->diagnostic_mode_start >= manager->config.diagnostic_mode_timeout) {
        // Auto-exit diagnostic mode
        EnhancedSafety_ExitDiagnosticMode(manager);
    }
}

bool EnhancedSafety_Init(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    // Clear manager
    memset(manager, 0, sizeof(EnhancedSafetyManager_t));
    
    // Initialize with defaults
    manager->config = default_config;
    manager->critical_data = default_critical_data;
    manager->critical_data.crc = calculate_crc16(&manager->critical_data);
    
    // Initialize module health
    for (int i = 0; i < WDG_MODULE_COUNT; i++) {
        manager->module_health[i].healthy = true;
        manager->module_health[i].degradation = DEGRADATION_NONE;
        manager->module_health[i].last_checkin = HAL_GetTick();
    }
    
    // Set initial state
    manager->current_state = SAFETY_STATE_INIT;
    manager->previous_state = SAFETY_STATE_INIT;
    manager->state_entry_time = HAL_GetTick();
    
    // Initialize fault history
    if (FaultHistory_Init() != true) {
        // Non-fatal - continue without fault history
    }
    
    // Transition to normal state
    transition_state(manager, SAFETY_STATE_NORMAL);
    
    manager->initialized = true;
    safety_state.active_manager = manager;
    
    return true;
}

bool EnhancedSafety_Deinit(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    // Ensure safe state
    transition_state(manager, SAFETY_STATE_SAFE_STOP);
    
    // Deinitialize fault history
    FaultHistory_Deinit();
    
    manager->initialized = false;
    safety_state.active_manager = NULL;
    
    return true;
}

SafetyState_t EnhancedSafety_Process(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL || !manager->initialized) {
        return SAFETY_STATE_ERROR;
    }
    
    // Prevent reentrancy
    if (safety_state.processing) {
        return manager->current_state;
    }
    safety_state.processing = true;
    
    // Update uptime
    manager->stats.system_uptime_ms = HAL_GetTick();
    
    // Check watchdogs
    check_watchdogs(manager);
    
    // Process recovery state machine
    process_recovery(manager);
    
    // Check diagnostic mode timeout
    check_diagnostic_timeout(manager);
    
    // Self-checkin to safety watchdog
    EnhancedSafety_WatchdogCheckin(manager, WDG_MODULE_SAFETY);
    
    safety_state.processing = false;
    return manager->current_state;
}

bool EnhancedSafety_ReportFault(EnhancedSafetyManager_t* manager, 
                                 FaultType_t fault_type,
                                 FaultSeverity_t severity,
                                 uint16_t error_code,
                                 uint32_t context)
{
    if (manager == NULL || !manager->initialized) {
        return false;
    }
    
    // Log fault
    FaultHistory_Log(fault_type, severity, error_code, context);
    
    // Handle based on severity
    switch (severity) {
        case FAULT_SEVERITY_INFO:
            // Just log - no action needed
            return true;
            
        case FAULT_SEVERITY_WARNING:
            // Log and potentially degrade
            if (manager->config.graceful_degradation_enabled) {
                if (manager->degradation == DEGRADATION_NONE) {
                    manager->degradation = DEGRADATION_LIGHT;
                }
            }
            return true;
            
        case FAULT_SEVERITY_ERROR:
            // Attempt recovery
            if (attempt_recovery(manager, fault_type)) {
                manager->recovery_status = RECOVERY_ATTEMPT_1;
                manager->stats.recovery_attempts++;
                
                transition_state(manager, SAFETY_STATE_RECOVERY);
                
                const struct {
                    FaultType_t fault;
                    RecoveryAction_t action;
                    uint8_t max_attempts;
                    SafetyState_t failure_state;
                }* mapping = get_recovery_mapping(fault_type);
                
                if (mapping != NULL) {
                    RecoveryResult_t result = execute_recovery(manager, mapping->action);
                    
                    if (result == RECOVERY_RESULT_SUCCESS) {
                        manager->recovery_status = RECOVERY_SUCCESS;
                        manager->stats.recovery_successes++;
                        FaultHistory_LogWithRecovery(fault_type, severity, error_code, context,
                                                      mapping->action, RECOVERY_RESULT_SUCCESS);
                    } else {
                        manager->recovery_status = RECOVERY_FAILED;
                        manager->stats.recovery_failures++;
                        FaultHistory_LogWithRecovery(fault_type, severity, error_code, context,
                                                      mapping->action, RECOVERY_RESULT_FAILED);
                        transition_state(manager, mapping->failure_state);
                    }
                }
            } else {
                // No recovery - enter degraded or safe stop
                const struct {
                    FaultType_t fault;
                    RecoveryAction_t action;
                    uint8_t max_attempts;
                    SafetyState_t failure_state;
                }* mapping = get_recovery_mapping(fault_type);
                
                if (mapping != NULL) {
                    transition_state(manager, mapping->failure_state);
                }
            }
            return true;
            
        case FAULT_SEVERITY_CRITICAL:
        case FAULT_SEVERITY_FATAL:
            // Immediate emergency stop
            EnhancedSafety_EmergencyStop(manager, "Critical fault");
            FaultHistory_Log(fault_type, severity, error_code, context);
            return false;
    }
    
    return true;
}

bool EnhancedSafety_EnterDiagnosticMode(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL || !manager->initialized) {
        return false;
    }
    
    manager->diagnostic_mode = true;
    manager->diagnostic_mode_start = HAL_GetTick();
    
    // Enable fault history diagnostic mode
    FaultHistory_SetDiagnosticMode(true);
    
    return true;
}

void EnhancedSafety_ExitDiagnosticMode(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return;
    }
    
    manager->diagnostic_mode = false;
    FaultHistory_SetDiagnosticMode(false);
}

bool EnhancedSafety_IsDiagnosticMode(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    return manager->diagnostic_mode;
}

SafetyState_t EnhancedSafety_GetState(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return SAFETY_STATE_ERROR;
    }
    
    return manager->current_state;
}

DegradationLevel_t EnhancedSafety_GetDegradationLevel(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return DEGRADATION_CRITICAL;
    }
    
    return manager->degradation;
}

RecoveryStatus_t EnhancedSafety_GetRecoveryStatus(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return RECOVERY_IDLE;
    }
    
    return manager->recovery_status;
}

bool EnhancedSafety_IsRecovering(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    return (manager->recovery_status == RECOVERY_ATTEMPT_1) ||
           (manager->recovery_status == RECOVERY_ATTEMPT_2) ||
           (manager->recovery_status == RECOVERY_ATTEMPT_3) ||
           (manager->recovery_status == RECOVERY_BACKOFF);
}

bool EnhancedSafety_RequestRecovery(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL || !manager->initialized) {
        return false;
    }
    
    if (manager->current_state == SAFETY_STATE_EMERGENCY ||
        manager->current_state == SAFETY_STATE_ERROR) {
        return false;  // Can't recover from these
    }
    
    manager->recovery_status = RECOVERY_ATTEMPT_1;
    manager->stats.recovery_attempts++;
    transition_state(manager, SAFETY_STATE_RECOVERY);
    
    return true;
}

void EnhancedSafety_EmergencyStop(EnhancedSafetyManager_t* manager, const char* reason)
{
    (void)reason;
    
    if (manager == NULL) {
        return;
    }
    
    manager->stats.emergency_stops++;
    transition_state(manager, SAFETY_STATE_EMERGENCY);
    
    // In a real implementation, would:
    // 1. Stop PWM immediately
    // 2. Set safe GPIO states
    // 3. Disable all outputs
    // 4. Log emergency event
}

bool EnhancedSafety_IsEmergencyStop(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    return (manager->current_state == SAFETY_STATE_EMERGENCY);
}

bool EnhancedSafety_ClearEmergencyStop(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    if (manager->current_state != SAFETY_STATE_EMERGENCY) {
        return false;
    }
    
    // Reset to safe stop first (manual verification required)
    transition_state(manager, SAFETY_STATE_SAFE_STOP);
    return true;
}

float EnhancedSafety_GetEffectiveDutyLimit(const EnhancedSafetyManager_t* manager,
                                            float requested_duty)
{
    if (manager == NULL) {
        return 0.0f;  // Safe default
    }
    
    float max_duty = manager->critical_data.max_duty_cycle;
    
    // Apply degradation limits
    switch (manager->degradation) {
        case DEGRADATION_NONE:
            break;
        case DEGRADATION_LIGHT:
            max_duty *= 0.9f;  // 90%
            break;
        case DEGRADATION_MODERATE:
            max_duty = manager->config.pwm_degradation_duty;  // 50%
            break;
        case DEGRADATION_SEVERE:
            max_duty *= 0.2f;  // 20%
            break;
        case DEGRADATION_CRITICAL:
            return 0.0f;  // No PWM
    }
    
    // Clamp to max duty
    if (requested_duty > max_duty) {
        return max_duty;
    }
    
    return requested_duty;
}

float EnhancedSafety_GetEffectiveCurrentLimit(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return 0.0f;  // Safe default
    }
    
    float max_current = manager->critical_data.max_current;
    
    // Apply degradation limits
    switch (manager->degradation) {
        case DEGRADATION_NONE:
            break;
        case DEGRADATION_LIGHT:
            max_current *= 0.9f;
            break;
        case DEGRADATION_MODERATE:
            max_current *= 0.7f;
            break;
        case DEGRADATION_SEVERE:
            max_current *= 0.5f;
            break;
        case DEGRADATION_CRITICAL:
            return 0.0f;
    }
    
    return max_current;
}

bool EnhancedSafety_WatchdogCheckin(EnhancedSafetyManager_t* manager, 
                                      WatchdogModule_t module)
{
    if (manager == NULL || module >= WDG_MODULE_COUNT) {
        return false;
    }
    
    manager->module_health[module].last_checkin = HAL_GetTick();
    return true;
}

bool EnhancedSafety_SetModuleHealth(EnhancedSafetyManager_t* manager,
                                     WatchdogModule_t module,
                                     bool healthy,
                                     DegradationLevel_t degradation)
{
    if (manager == NULL || module >= WDG_MODULE_COUNT) {
        return false;
    }
    
    manager->module_health[module].healthy = healthy;
    manager->module_health[module].degradation = degradation;
    
    return true;
}

ModuleHealth_t EnhancedSafety_GetModuleHealth(const EnhancedSafetyManager_t* manager,
                                                WatchdogModule_t module)
{
    ModuleHealth_t empty = {0};
    
    if (manager == NULL || module >= WDG_MODULE_COUNT) {
        return empty;
    }
    
    return manager->module_health[module];
}

bool EnhancedSafety_ValidateCriticalData(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    if (!manager->config.crc_validation_enabled) {
        return true;  // CRC disabled
    }
    
    uint16_t crc = calculate_crc16(&manager->critical_data);
    bool valid = (crc == manager->critical_data.crc);
    
    if (!valid) {
        safety_state.active_manager->stats.crc_failures++;
    }
    
    return valid;
}

bool EnhancedSafety_UpdateCriticalData(EnhancedSafetyManager_t* manager,
                                          const CriticalSafetyData_t* data)
{
    if (manager == NULL || data == NULL) {
        return false;
    }
    
    // Copy data and calculate CRC
    manager->critical_data = *data;
    manager->critical_data.crc = calculate_crc16(&manager->critical_data);
    
    return true;
}

uint16_t EnhancedSafety_CalculateCRC16(const CriticalSafetyData_t* data)
{
    if (data == NULL) {
        return 0;
    }
    
    return calculate_crc16(data);
}

void EnhancedSafety_GetStatistics(const EnhancedSafetyManager_t* manager,
                                   SafetyStatistics_t* stats)
{
    if (stats == NULL) {
        return;
    }
    
    if (manager == NULL) {
        memset(stats, 0, sizeof(SafetyStatistics_t));
        return;
    }
    
    *stats = manager->stats;
}

const SafetyConfig_t* EnhancedSafety_GetConfig(const EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return NULL;
    }
    
    return &manager->config;
}

bool EnhancedSafety_SetConfig(EnhancedSafetyManager_t* manager,
                               const SafetyConfig_t* config)
{
    if (manager == NULL || config == NULL) {
        return false;
    }
    
    // Validate config
    if (config->max_recovery_attempts > 10) {
        return false;  // Sanity check
    }
    
    if (config->recovery_backoff_ms < 1000 || config->recovery_backoff_ms > 60000) {
        return false;  // Backoff must be 1-60 seconds
    }
    
    manager->config = *config;
    return true;
}

const char* EnhancedSafety_GetStateString(SafetyState_t state)
{
    switch (state) {
        case SAFETY_STATE_INIT:         return "INIT";
        case SAFETY_STATE_NORMAL:       return "NORMAL";
        case SAFETY_STATE_DEGRADED_PWM: return "DEGRADED_PWM";
        case SAFETY_STATE_DEGRADED_ADC: return "DEGRADED_ADC";
        case SAFETY_STATE_RECOVERY:     return "RECOVERY";
        case SAFETY_STATE_SAFE_STOP:    return "SAFE_STOP";
        case SAFETY_STATE_EMERGENCY:    return "EMERGENCY";
        case SAFETY_STATE_ERROR:        return "ERROR";
        default:                        return "UNKNOWN";
    }
}

const char* EnhancedSafety_GetDegradationString(DegradationLevel_t level)
{
    switch (level) {
        case DEGRADATION_NONE:      return "NONE";
        case DEGRADATION_LIGHT:     return "LIGHT";
        case DEGRADATION_MODERATE:  return "MODERATE";
        case DEGRADATION_SEVERE:    return "SEVERE";
        case DEGRADATION_CRITICAL:  return "CRITICAL";
        default:                    return "UNKNOWN";
    }
}

const char* EnhancedSafety_GetRecoveryStatusString(RecoveryStatus_t status)
{
    switch (status) {
        case RECOVERY_IDLE:       return "IDLE";
        case RECOVERY_ATTEMPT_1:  return "ATTEMPT_1";
        case RECOVERY_ATTEMPT_2:  return "ATTEMPT_2";
        case RECOVERY_ATTEMPT_3:  return "ATTEMPT_3";
        case RECOVERY_BACKOFF:    return "BACKOFF";
        case RECOVERY_SUCCESS:    return "SUCCESS";
        case RECOVERY_FAILED:     return "FAILED";
        default:                  return "UNKNOWN";
    }
}

const char* EnhancedSafety_GetModuleString(WatchdogModule_t module)
{
    switch (module) {
        case WDG_MODULE_MAIN:    return "MAIN";
        case WDG_MODULE_PWM:     return "PWM";
        case WDG_MODULE_ADC:     return "ADC";
        case WDG_MODULE_COMM:    return "COMM";
        case WDG_MODULE_SAFETY:  return "SAFETY";
        default:                 return "UNKNOWN";
    }
}

uint16_t EnhancedSafety_FormatStatus(const EnhancedSafetyManager_t* manager,
                                      char* buffer, uint16_t size)
{
    if (manager == NULL || buffer == NULL || size == 0) {
        return 0;
    }
    
    uint16_t written = 0;
    
    written += snprintf(buffer + written, size - written,
        "Safety System Status:\r\n");
    written += snprintf(buffer + written, size - written,
        "=====================\r\n");
    written += snprintf(buffer + written, size - written,
        "State:      %s\r\n",
        EnhancedSafety_GetStateString(manager->current_state));
    written += snprintf(buffer + written, size - written,
        "Previous:   %s\r\n",
        EnhancedSafety_GetStateString(manager->previous_state));
    written += snprintf(buffer + written, size - written,
        "Degradation: %s\r\n",
        EnhancedSafety_GetDegradationString(manager->degradation));
    written += snprintf(buffer + written, size - written,
        "Recovery:   %s\r\n",
        EnhancedSafety_GetRecoveryStatusString(manager->recovery_status));
    written += snprintf(buffer + written, size - written,
        "Diagnostic: %s\r\n",
        manager->diagnostic_mode ? "ON" : "OFF");
    written += snprintf(buffer + written, size - written,
        "CRC Valid:  %s\r\n",
        EnhancedSafety_ValidateCriticalData(manager) ? "YES" : "NO");
    
    written += snprintf(buffer + written, size - written,
        "\r\nModule Health:\r\n");
    for (int i = 0; i < WDG_MODULE_COUNT; i++) {
        const ModuleHealth_t* health = &manager->module_health[i];
        written += snprintf(buffer + written, size - written,
            "  %s: %s (%s)\r\n",
            EnhancedSafety_GetModuleString(i),
            health->healthy ? "OK" : "FAIL",
            EnhancedSafety_GetDegradationString(health->degradation));
    }
    
    written += snprintf(buffer + written, size - written,
        "\r\nStatistics:\r\n");
    written += snprintf(buffer + written, size - written,
        "  Recoveries: %lu/%lu (failed: %lu)\r\n",
        (unsigned long)manager->stats.recovery_successes,
        (unsigned long)manager->stats.recovery_attempts,
        (unsigned long)manager->stats.recovery_failures);
    written += snprintf(buffer + written, size - written,
        "  Degradations: %lu\r\n",
        (unsigned long)manager->stats.degradations_triggered);
    written += snprintf(buffer + written, size - written,
        "  Emergencies: %lu\r\n",
        (unsigned long)manager->stats.emergency_stops);
    written += snprintf(buffer + written, size - written,
        "  CRC Failures: %lu\r\n",
        (unsigned long)manager->stats.crc_failures);
    written += snprintf(buffer + written, size - written,
        "  Watchdog Timeouts: %lu\r\n",
        (unsigned long)manager->stats.watchdog_timeouts);
    
    return written;
}

bool EnhancedSafety_SelfTest(EnhancedSafetyManager_t* manager)
{
    if (manager == NULL) {
        return false;
    }
    
    // Test 1: Check initialized
    if (!manager->initialized) {
        return false;
    }
    
    // Test 2: Validate CRC calculation
    CriticalSafetyData_t test_data = default_critical_data;
    test_data.crc = 0;
    uint16_t crc = calculate_crc16(&test_data);
    if (crc == 0) {
        return false;
    }
    
    // Test 3: Check critical data
    if (!EnhancedSafety_ValidateCriticalData(manager)) {
        return false;
    }
    
    // Test 4: Verify modules
    for (int i = 0; i < WDG_MODULE_COUNT; i++) {
        if (manager->module_health[i].last_checkin == 0) {
            return false;
        }
    }
    
    return true;
}
