/**
 * @file cli_commands.h
 * @brief Command Line Interface command handlers
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

// Built-in commands
bool cmd_status(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_config(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_monitor(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_pwm(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_calibrate(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_errors(Adaptive_UART_t* uart, int argc, const char* argv[]);
bool cmd_help(Adaptive_UART_t* uart, int argc, const char* argv[]);

#endif // CLI_COMMANDS_H
