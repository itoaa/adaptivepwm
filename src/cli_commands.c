/**
 * @file cli_commands.c
 * @brief CLI command implementations with authentication
 * @security Authentication enforced for privileged commands
 * @safety Commands validated before execution
 * 
 * Security Task: PWM-ARCH-004
 * Added diagnostic commands for Enhanced Safety System
 * Added flash wear leveling statistics command
 * 
 * Performance Task: PWM-ARCH-009
 * Added profile command for performance profiling
 */

#include "cli_commands.h"
#include "cli_auth.h"
#include "hal_pwm.h"
#include "hal_adc.h"
#include "param_calc.h"
#include "error_handler.h"
#include "temperature_monitor.h"
#include "fault_history.h"
#include "enhanced_safety.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

extern Adaptive_PWM_t pwm_handle;
extern Adaptive_ADC_t adc_handle;
extern CalculatedParams_t calc_params;
extern ErrorManager_t error_manager;
extern TempMonitor_t temp_monitor;
extern EnhancedSafetyManager_t safety_manager;

// Forward declarations for new commands (PWM-ARCH-004)
static bool cmd_wear(Adaptive_UART_t* uart, int argc, const char* argv[]);

// Command table with authentication requirements
static const Command_t commands[] = {
    // Public commands (no auth required)
    {"login",     "Authenticate with password",  "login <password>",          cmd_login,         false, false},
    {"authstatus","Show authentication status",  "authstatus",                cmd_authstatus,    false, false},
    {"help",      "Show this help",              "help [command]",            cmd_help,          false, false},
    
    // Protected commands (auth required)
    {"status",    "Show system status",          "status [adc|pwm|params]",   cmd_status,        true,  false},
    {"config",    "Configure system",            "config <param> <value>",    cmd_config,        true,  false},
    {"monitor",   "Real-time monitoring",        "monitor [duration]",        cmd_monitor,       true,  false},
    {"pwm",       "PWM control",                 "pwm <duty|start|stop>",     cmd_pwm,           true,  false},
    {"calibrate", "Calibrate ADC",                 "calibrate <vin> <vout>",    cmd_calibrate,     true,  false},
    {"errors",    "Show error log",              "errors [clear]",            cmd_errors,        true,  false},
    {"logout",    "Logout from session",         "logout",                    cmd_logout,        true,  false},
    {"passwd",    "Change password",             "passwd [old] <new>",        cmd_passwd,        true,  false},
    
    // Diagnostic commands (PWM-ARCH-004)
    {"faults",    "Show fault history",          "faults [clear|stats]",      cmd_faults,        true,  false},
    {"wear",      "Show flash wear statistics",  "wear [validate]",           cmd_wear,          true,  false},
    {"diagnostic","Enter diagnostic mode",       "diagnostic [on|off]",       cmd_diagnostic,    true,  false},
    {"safety",    "Show safety system status",   "safety [status|test]",      cmd_safety,        true,  false},
    {"recovery",  "Trigger/request recovery",    "recovery [request]",        cmd_recovery,      true,  false},
    {"maintenance","Show maintenance prediction", "maintenance",               cmd_maintenance,   true,  false},
    
    // Performance profiling command (PWM-ARCH-009)
//     {"profile",   "Show performance profiling",  "profile [reset]",           cmd_profile,       true,  false},
    
    {NULL, NULL, NULL, NULL, false, false}
};

bool CLI_Init(void)
{
    // Initialize authentication module
    if (!CLI_Auth_Init()) {
        return false;
    }
    
    // Initialize fault history
    FaultHistory_Init();
    
    // Initialize enhanced safety
    EnhancedSafety_Init(&safety_manager);
    
    return true;
}

static const char** tokenize(const char* cmd, int* argc)
{
    static const char* argv[16];
    static char buffer[256];
    
    strncpy(buffer, cmd, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    *argc = 0;
    char* token = strtok(buffer, " \t\r\n");
    while (token != NULL && *argc < 16) {
        argv[(*argc)++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    
    return argv;
}

bool CLI_CommandRequiresAuth(const char* cmd)
{
    if (cmd == NULL) return false;
    
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            return commands[i].requires_auth;
        }
    }
    
    return false;  // Unknown commands treated as public
}

bool CLI_IsAuthenticated(void)
{
    return CLI_Auth_IsAuthenticated();
}

bool CLI_ProcessCommand(Adaptive_UART_t* uart, const char* cmd)
{
    if (uart == NULL || cmd == NULL) return false;
    
    // Skip leading whitespace
    while (isspace(*cmd)) cmd++;
    if (*cmd == '\0') return false;
    
    int argc;
    const char** argv = tokenize(cmd, &argc);
    if (argc == 0) return false;
    
    // Find command
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            // Check authentication requirement
            if (commands[i].requires_auth && !CLI_Auth_IsAuthenticated()) {
                Adaptive_UART_Printf(uart, "Authentication required. Use: login <password>\r\n");
                return false;
            }
            
            // Execute command
            bool result = commands[i].handler(uart, argc, argv);
            
            // Refresh session on any successful command
            if (result && CLI_Auth_IsAuthenticated()) {
                CLI_Auth_RefreshSession();
            }
            
            return result;
        }
    }
    
    Adaptive_UART_Printf(uart, "Unknown command: %s\r\n", argv[0]);
    return false;
}

uint16_t CLI_GetHelp(char* buffer, uint16_t size)
{
    uint16_t written = snprintf(buffer, size, 
        "AdaptivePWM CLI v" ADAPTIVEPWM_VERSION_STRING "\r\n");
    written += snprintf(buffer + written, size - written,
        "Authentication: %s\r\n\r\n",
        CLI_Auth_IsAuthenticated() ? "Authenticated" : 
        (CLI_Auth_IsEnabled() ? "Required" : "Disabled"));
    
    written += snprintf(buffer + written, size - written,
        "Commands:\r\n");
    
    for (int i = 0; commands[i].name != NULL; i++) {
        if (!commands[i].hidden) {
            const char* lock = commands[i].requires_auth ? "🔒 " : "  ";
            written += snprintf(buffer + written, size - written,
                "%s%-12s %s\r\n", lock, commands[i].name, commands[i].description);
        }
    }
    
    written += snprintf(buffer + written, size - written,
        "\r\nType 'help <command>' for detailed usage.\r\n");
    
    if (CLI_Auth_IsEnabled() && !CLI_Auth_IsAuthenticated()) {
        written += snprintf(buffer + written, size - written,
            "\r\nNote: 🔒 commands require authentication.\r\n");
    }
    
    return written;
}

// Command implementations
bool cmd_status(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    (void)argc; (void)argv; // Unused for now
    
    if (argc > 1 && strcmp(argv[1], "adc") == 0) {
        ADC_Measurement_t meas;
        if (Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
            Adaptive_UART_Printf(uart, "ADC:\r\n");
            Adaptive_UART_Printf(uart, "  Vin:  %.3f V\r\n", meas.vin);
            Adaptive_UART_Printf(uart, "  Vout: %.3f V\r\n", meas.vout);
            Adaptive_UART_Printf(uart, "  I:    %.3f A\r\n", meas.current);
            Adaptive_UART_Printf(uart, "  T:    %.1f C\r\n", meas.temperature);
        } else {
            Adaptive_UART_Printf(uart, "ADC not ready\r\n");
        }
    } else if (argc > 1 && strcmp(argv[1], "pwm") == 0) {
        Adaptive_UART_Printf(uart, "PWM:\r\n");
        Adaptive_UART_Printf(uart, "  Freq: %lu Hz\r\n", Adaptive_PWM_GetFrequency(&pwm_handle));
        Adaptive_UART_Printf(uart, "  Duty: %.2f %%\r\n", Adaptive_PWM_GetDuty(&pwm_handle) * 100);
    } else if (argc > 1 && strcmp(argv[1], "params") == 0) {
        if (calc_params.valid) {
            Adaptive_UART_Printf(uart, "Parameters:\r\n");
            Adaptive_UART_Printf(uart, "  L:    %.3f mH\r\n", calc_params.inductance_mH);
            Adaptive_UART_Printf(uart, "  C:    %.3f uF\r\n", calc_params.capacitance_uF);
            Adaptive_UART_Printf(uart, "  ESR:  %.3f mOhm\r\n", calc_params.esr_mOhm);
            Adaptive_UART_Printf(uart, "  dI:   %.3f A\r\n", calc_params.ripple_current);
            Adaptive_UART_Printf(uart, "  dV:   %.3f V\r\n", calc_params.ripple_voltage);
        } else {
            Adaptive_UART_Printf(uart, "Parameters not calculated\r\n");
        }
    } else {
        Adaptive_UART_Printf(uart, "System Status:\r\n");
        Adaptive_UART_Printf(uart, "  PWM: %s\r\n", pwm_handle.is_running ? "Running" : "Stopped");
        Adaptive_UART_Printf(uart, "  Temp: %.1fC (%s)\r\n", 
            temp_monitor.current_temp,
            TempMonitor_IsSafe(&temp_monitor) ? "OK" : "ALERT");
        Adaptive_UART_Printf(uart, "  Auth: %s\r\n",
            CLI_Auth_IsAuthenticated() ? "Authenticated" : "Unauthenticated");
        Adaptive_UART_Printf(uart, "  Safety: %s\r\n",
            EnhancedSafety_GetStateString(EnhancedSafety_GetState(&safety_manager)));
    }
    return true;
}

bool cmd_config(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    (void)argc; (void)argv; // Unused for now
    Adaptive_UART_Printf(uart, "Config - Not implemented\r\n");
    return true;
}

bool cmd_monitor(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    int duration = 10;
    if (argc > 1) duration = atoi(argv[1]);
    if (duration < 1) duration = 1;
    if (duration > 60) duration = 60;
    
    Adaptive_UART_Printf(uart, "Monitoring for %d seconds...\r\n", duration);
    
    // Note: Real implementation would loop and print periodically
    // This is a placeholder
    Adaptive_UART_Printf(uart, "(Monitor would run here)\r\n");
    
    return true;
}

bool cmd_pwm(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc < 2) {
        Adaptive_UART_Printf(uart, "Usage: pwm [duty|start|stop]\r\n");
        return false;
    }
    
    if (strcmp(argv[1], "start") == 0) {
        if (Adaptive_PWM_Start(&pwm_handle)) {
            Adaptive_UART_Printf(uart, "PWM started\r\n");
        } else {
            Adaptive_UART_Printf(uart, "PWM start failed\r\n");
        }
    } else if (strcmp(argv[1], "stop") == 0) {
        if (Adaptive_PWM_Stop(&pwm_handle)) {
            Adaptive_UART_Printf(uart, "PWM stopped\r\n");
        } else {
            Adaptive_UART_Printf(uart, "PWM stop failed\r\n");
        }
    } else {
        float duty = atof(argv[1]);
        if (duty < 0) duty = 0;
        if (duty > 100) duty = 100;
        duty /= 100.0f;
        
        // Apply safety limits
        duty = EnhancedSafety_GetEffectiveDutyLimit(&safety_manager, duty);
        
        if (Adaptive_PWM_SetDuty(&pwm_handle, duty)) {
            Adaptive_UART_Printf(uart, "Duty set to %.1f%%\r\n", duty * 100);
        } else {
            Adaptive_UART_Printf(uart, "Failed to set duty\r\n");
        }
    }
    return true;
}

bool cmd_calibrate(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc < 3) {
        Adaptive_UART_Printf(uart, "Usage: calibrate [vin] [vout]\r\n");
        return false;
    }
    
    float vin = atof(argv[1]);
    float vout = atof(argv[2]);
    
    Adaptive_UART_Printf(uart, "Calibrating with Vin=%.2fV Vout=%.2fV\r\n", vin, vout);
    
    if (Adaptive_ADC_Calibrate(&adc_handle, vin, vout, 0.0f)) {
        Adaptive_UART_Printf(uart, "Calibration saved\r\n");
    } else {
        Adaptive_UART_Printf(uart, "Calibration failed\r\n");
    }
    
    return true;
}

bool cmd_errors(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "clear") == 0) {
        Error_ClearFault(&error_manager);
        Adaptive_UART_Printf(uart, "Error log cleared\r\n");
    } else {
        char buffer[512];
        Error_GetLog(&error_manager, buffer, sizeof(buffer));
        Adaptive_UART_SendString(uart, buffer);
    }
    return true;
}

bool cmd_help(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1) {
        // Show help for specific command
        for (int i = 0; commands[i].name != NULL; i++) {
            if (strcmp(argv[1], commands[i].name) == 0) {
                Adaptive_UART_Printf(uart, "Command: %s\r\n", commands[i].name);
                Adaptive_UART_Printf(uart, "Description: %s\r\n", commands[i].description);
                Adaptive_UART_Printf(uart, "Usage: %s\r\n", commands[i].usage);
                if (commands[i].requires_auth) {
                    Adaptive_UART_Printf(uart, "Requires: Authentication\r\n");
                }
                return true;
            }
        }
        Adaptive_UART_Printf(uart, "Unknown command: %s\r\n", argv[1]);
        return false;
    }
    
    // Show general help
    char buffer[512];
    CLI_GetHelp(buffer, sizeof(buffer));
    Adaptive_UART_SendString(uart, buffer);
    return true;
}

// Diagnostic command implementations (PWM-ARCH-004)
bool cmd_faults(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "clear") == 0) {
            if (FaultHistory_Clear()) {
                Adaptive_UART_Printf(uart, "Fault history cleared\r\n");
            } else {
                Adaptive_UART_Printf(uart, "Failed to clear fault history\r\n");
            }
            return true;
        } else if (strcmp(argv[1], "stats") == 0) {
            FaultStatistics_t stats;
            FaultHistory_GetStatistics(&stats);
            
            Adaptive_UART_Printf(uart, "Fault Statistics:\r\n");
            Adaptive_UART_Printf(uart, "  Total faults: %lu\r\n", (unsigned long)stats.total_faults);
            Adaptive_UART_Printf(uart, "  Total recoveries: %lu\r\n", (unsigned long)stats.total_recoveries);
            Adaptive_UART_Printf(uart, "  Failed recoveries: %lu\r\n", (unsigned long)stats.failed_recoveries);
            Adaptive_UART_Printf(uart, "  System resets: %lu\r\n", (unsigned long)stats.system_resets);
            Adaptive_UART_Printf(uart, "  Fault rate (1h): %lu\r\n", (unsigned long)stats.fault_rate_1h);
            Adaptive_UART_Printf(uart, "  Fault rate (24h): %lu\r\n", (unsigned long)stats.fault_rate_24h);
            
            Adaptive_UART_Printf(uart, "\r\nFaults by type:\r\n");
            for (int i = 0; i < FAULT_TYPE_COUNT; i++) {
                if (stats.faults_by_type[i] > 0) {
                    Adaptive_UART_Printf(uart, "  %s: %lu\r\n",
                        FaultHistory_GetTypeString(i),
                        (unsigned long)stats.faults_by_type[i]);
                }
            }
            return true;
        }
    }
    
    // Show fault log
    char buffer[1024];
    uint32_t count = FaultHistory_GetCount();
    
    if (count == 0) {
        Adaptive_UART_Printf(uart, "No faults recorded\r\n");
        return true;
    }
    
    uint16_t written = FaultHistory_GetLogText(buffer, sizeof(buffer), 20);
    Adaptive_UART_SendString(uart, buffer);
    
    if (count > 20) {
        Adaptive_UART_Printf(uart, "... and %lu more entries\r\n", 
            (unsigned long)(count - 20));
    }
    
    return true;
}

/**
 * @brief Flash wear statistics command (PWM-ARCH-004)
 * 
 * Displays flash wear leveling statistics and optionally
 * validates wear leveling integrity.
 */
bool cmd_wear(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "validate") == 0) {
        // Validate wear leveling integrity
        uint32_t errors = 0;
        bool valid = FaultHistory_ValidateWearLeveling(&errors);
        
        Adaptive_UART_Printf(uart, "Wear Leveling Validation:\r\n");
        Adaptive_UART_Printf(uart, "========================\r\n");
        
        if (valid) {
            Adaptive_UART_Printf(uart, "Status: PASSED\r\n");
            Adaptive_UART_Printf(uart, "No inconsistencies found.\r\n");
        } else {
            Adaptive_UART_Printf(uart, "Status: FAILED\r\n");
            Adaptive_UART_Printf(uart, "Errors found: %lu\r\n", (unsigned long)errors);
            Adaptive_UART_Printf(uart, "\r\nRecommend clearing fault history.\r\n");
        }
        
        return true;
    }
    
    // Get and display wear statistics
    FlashWearStats_t stats;
    FaultHistory_GetWearStats(&stats);
    
    char buffer[1024];
    uint16_t written = FaultHistory_FormatWearStats(&stats, buffer, sizeof(buffer));
    
    Adaptive_UART_SendString(uart, buffer);
    
    // Add usage hint
    Adaptive_UART_Printf(uart, "\r\nUse 'wear validate' to check integrity.\r\n");
    
    return true;
}

bool cmd_diagnostic(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "on") == 0) {
            if (EnhancedSafety_EnterDiagnosticMode(&safety_manager)) {
                Adaptive_UART_Printf(uart, "Diagnostic mode ENABLED (5 min timeout)\r\n");
            } else {
                Adaptive_UART_Printf(uart, "Failed to enter diagnostic mode\r\n");
            }
            return true;
        } else if (strcmp(argv[1], "off") == 0) {
            EnhancedSafety_ExitDiagnosticMode(&safety_manager);
            Adaptive_UART_Printf(uart, "Diagnostic mode DISABLED\r\n");
            return true;
        }
    }
    
    // Show diagnostic mode status
    bool diag = EnhancedSafety_IsDiagnosticMode(&safety_manager);
    Adaptive_UART_Printf(uart, "Diagnostic mode: %s\r\n", diag ? "ON" : "OFF");
    
    if (diag) {
        Adaptive_UART_Printf(uart, "Extended logging active\r\n");
        Adaptive_UART_Printf(uart, "Will auto-disable after 5 minutes\r\n");
    }
    
    return true;
}

bool cmd_safety(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        Adaptive_UART_Printf(uart, "Running safety self-test...\r\n");
        
        if (EnhancedSafety_SelfTest(&safety_manager)) {
            Adaptive_UART_Printf(uart, "Self-test PASSED\r\n");
        } else {
            Adaptive_UART_Printf(uart, "Self-test FAILED\r\n");
        }
        return true;
    }
    
    // Show safety status
    char buffer[1024];
    uint16_t written = EnhancedSafety_FormatStatus(&safety_manager, buffer, sizeof(buffer));
    Adaptive_UART_SendString(uart, buffer);
    
    return true;
}

bool cmd_recovery(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "request") == 0) {
        RecoveryStatus_t status = EnhancedSafety_GetRecoveryStatus(&safety_manager);
        
        if (status == RECOVERY_IDLE) {
            if (EnhancedSafety_RequestRecovery(&safety_manager)) {
                Adaptive_UART_Printf(uart, "Recovery initiated\r\n");
            } else {
                Adaptive_UART_Printf(uart, "Recovery not possible in current state\r\n");
            }
        } else {
            Adaptive_UART_Printf(uart, "Recovery already in progress: %s\r\n",
                EnhancedSafety_GetRecoveryStatusString(status));
        }
        return true;
    }
    
    // Show recovery status
    RecoveryStatus_t status = EnhancedSafety_GetRecoveryStatus(&safety_manager);
    SafetyStatistics_t stats;
    EnhancedSafety_GetStatistics(&safety_manager, &stats);
    
    Adaptive_UART_Printf(uart, "Recovery Status:\r\n");
    Adaptive_UART_Printf(uart, "  Current: %s\r\n", 
        EnhancedSafety_GetRecoveryStatusString(status));
    Adaptive_UART_Printf(uart, "  Recovering: %s\r\n",
        EnhancedSafety_IsRecovering(&safety_manager) ? "YES" : "NO");
    Adaptive_UART_Printf(uart, "\r\nStatistics:\r\n");
    Adaptive_UART_Printf(uart, "  Attempts: %lu\r\n", (unsigned long)stats.recovery_attempts);
    Adaptive_UART_Printf(uart, "  Successes: %lu\r\n", (unsigned long)stats.recovery_successes);
    Adaptive_UART_Printf(uart, "  Failures: %lu\r\n", (unsigned long)stats.recovery_failures);
    
    return true;
}

bool cmd_maintenance(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    (void)argc; (void)argv;
    
    MaintenancePrediction_t prediction;
    FaultHistory_GetMaintenancePrediction(&prediction);
    
    Adaptive_UART_Printf(uart, "Maintenance Prediction:\r\n");
    Adaptive_UART_Printf(uart, "=======================\r\n");
    Adaptive_UART_Printf(uart, "Health Score: %.1f%%\r\n", prediction.health_score);
    Adaptive_UART_Printf(uart, "Status: %s\r\n", 
        prediction.maintenance_recommended ? "MAINTENANCE REQUIRED" : "OK");
    
    if (prediction.maintenance_recommended) {
        Adaptive_UART_Printf(uart, "\r\n⚠️  Maintenance is recommended!\r\n");
        Adaptive_UART_Printf(uart, "Primary concern: %s\r\n", prediction.primary_concern);
    } else {
        Adaptive_UART_Printf(uart, "\r\nEstimated maintenance in: %lu days\r\n",
            (unsigned long)prediction.days_until_maintenance);
    }
    
    Adaptive_UART_Printf(uart, "\r\nDegradation rate: %.2f%%/day\r\n", 
        prediction.degradation_rate);
    
    return true;
}

// Performance profiling command (PWM-ARCH-009)
// Performance profiling stub (PWM-ARCH-009 - profiler not included in this build)
bool cmd_profile(Adaptive_UART_t* uart, int argc, const char* argv[])
{
    Adaptive_UART_Printf(uart, "Performance profiling not available in this build\r\n");
    return true;
}

