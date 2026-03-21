/**
 * @file cli_commands.c
 * @brief CLI command implementations
 */

#include "cli_commands.h"
#include "hal_pwm.h"
#include "hal_adc.h"
#include "param_calc.h"
#include "error_handler.h"
#include "temperature_monitor.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

extern Adaptive_PWM_t pwm_handle;
extern Adaptive_ADC_t adc_handle;
extern CalculatedParams_t calc_params;
extern ErrorManager_t error_manager;
extern TempMonitor_t temp_monitor;

static const Command_t commands[] = {
    {"status",    "Show system status",          "status [adc|pwm|params]",     cmd_status},
    {"config",    "Configure system",            "config <param> <value>",    cmd_config},
    {"monitor",   "Real-time monitoring",          "monitor [duration]",        cmd_monitor},
    {"pwm",       "PWM control",                 "pwm <duty|start|stop>",     cmd_pwm},
    {"calibrate", "Calibrate ADC",                 "calibrate <vin> <vout>",    cmd_calibrate},
    {"errors",    "Show error log",              "errors [clear]",             cmd_errors},
    {"help",      "Show this help",              "help [command]",            cmd_help},
    {NULL, NULL, NULL, NULL}
};

bool CLI_Init(void)
{
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
            return commands[i].handler(uart, argc, argv);
        }
    }
    
    Adaptive_UART_Printf(uart, "Unknown command: %s\r\n", argv[0]);
    return false;
}

uint16_t CLI_GetHelp(char* buffer, uint16_t size)
{
    uint16_t written = snprintf(buffer, size, "Commands:\r\n");
    
    for (int i = 0; commands[i].name != NULL; i++) {
        written += snprintf(buffer + written, size - written,
            "  %-12s %s\r\n", commands[i].name, commands[i].description);
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
    (void)argc; (void)argv; // Unused for now
    char buffer[512];
    CLI_GetHelp(buffer, sizeof(buffer));
    Adaptive_UART_SendString(uart, buffer);
    return true;
}
