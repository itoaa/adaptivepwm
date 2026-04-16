/**
 * @file cli_commands.h
 * @brief Command Line Interface command handlers
 * 
 * Security Task: PWM-ARCH-004
 * Includes diagnostic commands for Enhanced Safety System
 */

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_uart.h"

// Command callbacks
typedef bool (*CommandHandler_t)(Adaptive_UART_t* uart, int argc, const char* argv[]);

typedef struct {
    const char* name;
    const char* description;
    const char* usage;
    CommandHandler_t handler;
    bool requires_auth;      // Requires authentication to execute
    bool hidden;             // Hidden from help (admin commands)
} Command_t;

/**
 * @brief Initialize CLI
 * @return true if successful
 */
bool CLI_Init(void);

/**
 * @brief Process command string
 * @param uart UART handle
 * @param cmd Command string
 * @return true if command executed
 */
bool CLI_ProcessCommand(Adaptive_UART_t* uart, const char* cmd);

/**
 * @brief Get command list
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of bytes written
 */
uint16_t CLI_GetHelp(char* buffer, uint16_t size);

/**
 * @brief Check if authentication is required for a command
 * @param cmd Command name
 * @return true if authentication required
 */
bool CLI_CommandRequiresAuth(const char* cmd);

/**
 * @brief Check if user is authenticated for CLI
 * @return true if authenticated or auth disabled
 */
bool CLI_IsAuthenticated(void);

// Built-in commands
bool cmd_status(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_config(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_monitor(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_pwm(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_calibrate(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_errors(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_help(Adaptive_UART_t* uart, int argc, const char* argv[]);

// Authentication commands (from cli_auth.h)
bool cmd_login(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_logout(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_passwd(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_authstatus(Adaptive_UART_t* uart, int argc, const char* argv[]);

// Diagnostic commands (PWM-ARCH-004)
bool cmd_faults(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_diagnostic(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_safety(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_recovery(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_maintenance(Adaptive_UART_t* uart, int argc, const char* argv[]);

#endif // CLI_COMMANDS_H
