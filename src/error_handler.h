/**
 * @file error_handler.h
 * @brief System error handling
 * 
 * Comprehensive error management with safe shutdown
 * and error logging capabilities.
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// Error codes
#define ERR_NONE                0x0000
#define ERR_OVER_VOLTAGE        0x0001
#define ERR_UNDER_VOLTAGE       0x0002
#define ERR_OVER_CURRENT        0x0003
#define ERR_OVER_TEMP           0x0004
#define ERR_INVALID_PARAMS      0x0005
#define ERR_PWM_FAULT           0x0006
#define ERR_ADC_FAILURE         0x0007
#define ERR_WATCHDOG_TIMEOUT    0x0008
#define ERR_FREERTOS_ASSERT     0x0009
#define ERR_CLI_AUTH_FAILURE    0x000A
#define ERR_FLASH_WRITE_FAIL    0x000B

typedef enum {
    SEVERITY_INFO,      // Log only
    SEVERITY_WARNING,   // Log + notification
    SEVERITY_ERROR,     // Pause operation, log
    SEVERITY_CRITICAL   // Emergency stop, log
} ErrorSeverity_t;

typedef struct {
    uint16_t code;
    ErrorSeverity_t severity;
    uint32_t timestamp;
    const char* message;
    uint32_t data;
} ErrorEvent_t;

// Error log (circular buffer)
#define ERROR_LOG_SIZE      16

typedef struct {
    ErrorEvent_t events[ERROR_LOG_SIZE];
    uint8_t write_index;
    uint8_t count;
    uint32_t error_count[16];
    bool system_fault;
} ErrorManager_t;

/**
 * @brief Initialize error handler
 * @param manager Error manager
 */
void Error_Init(ErrorManager_t* manager);

/**
 * @brief Report an error
 * @param manager Error manager
 * @param code Error code
 * @param severity Error severity
 * @param message Human-readable message
 * @param data Additional data
 */
void Error_Report(ErrorManager_t* manager, uint16_t code, ErrorSeverity_t severity, 
                  const char* message, uint32_t data);

/**
 * @brief Handle critical error - stops system
 * @param manager Error manager
 * @param code Error code
 * @param message Error message
 */
void Error_Critical(ErrorManager_t* manager, uint16_t code, const char* message);

/**
 * @brief Get last error
 * @param manager Error manager
 * @return Pointer to last error event, NULL if none
 */
const ErrorEvent_t* Error_GetLast(const ErrorManager_t* manager);

/**
 * @brief Get error count for specific code
 * @param manager Error manager
 * @param code Error code
 * @return Number of occurrences
 */
uint32_t Error_GetCount(const ErrorManager_t* manager, uint16_t code);

/**
 * @brief Check if system is in fault state
 * @param manager Error manager
 * @return true if fault
 */
bool Error_IsFault(const ErrorManager_t* manager);

/**
 * @brief Clear fault state (after manual intervention)
 * @param manager Error manager
 */
void Error_ClearFault(ErrorManager_t* manager);

/**
 * @brief Get error log as formatted string
 * @param manager Error manager
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of bytes written
 */
uint16_t Error_GetLog(const ErrorManager_t* manager, char* buffer, uint16_t size);

#endif // ERROR_HANDLER_H
