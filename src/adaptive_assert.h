/**
 * @file adaptive_assert.h
 * @brief Runtime assertions and debugging macros
 */

#ifndef ADAPTIVE_ASSERT_H
#define ADAPTIVE_ASSERT_H

#include "config.h"
#include "error_handler.h"
#include "hal_uart.h"
#include <stdio.h>

// External UART for debug output
extern Adaptive_UART_t uart_handle;

// =============================================================================
// ASSERTIONS
// =============================================================================

#if ASSERT_ENABLED
    #define ADAPTIVE_ASSERT(cond) \
        do { \
            if (!(cond)) { \
                Adaptive_UART_Printf(&uart_handle, \
                    "ASSERT FAILED: %s:%d: %s\r\n", __FILE__, __LINE__, #cond); \
                Error_Critical(NULL, ERR_FREERTOS_ASSERT, #cond); \
            } \
        } while(0)
    
    #define ADAPTIVE_ASSERT_MSG(cond, msg) \
        do { \
            if (!(cond)) { \
                Adaptive_UART_Printf(&uart_handle, \
                    "ASSERT FAILED: %s:%d: %s\r\n", __FILE__, __LINE__, msg); \
                Error_Critical(NULL, ERR_FREERTOS_ASSERT, msg); \
            } \
        } while(0)
#else
    #define ADAPTIVE_ASSERT(cond) ((void)0)
    #define ADAPTIVE_ASSERT_MSG(cond, msg) ((void)0)
#endif

// =============================================================================
// DEBUG OUTPUT
// =============================================================================

#if DEBUG_PRINT_ENABLED
    #define DEBUG_PRINT(fmt, ...) \
        Adaptive_UART_Printf(&uart_handle, "[DBG] " fmt "\r\n", ##__VA_ARGS__)
    
    #define DEBUG_PRINT_EVERY_N(n, fmt, ...) \
        do { \
            static int _debug_cnt = 0; \
            if (++_debug_cnt >= (n)) { \
                _debug_cnt = 0; \
                DEBUG_PRINT(fmt, ##__VA_ARGS__); \
            } \
        } while(0)
#else
    #define DEBUG_PRINT(fmt, ...) ((void)0)
    #define DEBUG_PRINT_EVERY_N(n, fmt, ...) ((void)0)
#endif

// =============================================================================
// ERROR HANDLING HELPERS
// =============================================================================

#define ADAPTIVE_CHECK(cond, err_code, severity, msg) \
    do { \
        if (!(cond)) { \
            Error_Report(&error_manager, err_code, severity, msg, 0); \
        } \
    } while(0)

#define ADAPTIVE_CHECK_RETURN(cond, err_code, msg) \
    do { \
        if (!(cond)) { \
            Error_Report(&error_manager, err_code, SEVERITY_ERROR, msg, 0); \
            return false; \
        } \
    } while(0)

// =============================================================================
// STATISTICS COUNTERS
// =============================================================================

#ifdef DEBUG
    #define STATS_INCREMENT(counter) \
        do { extern volatile uint32_t counter; counter++; } while(0)
    
    #define STATS_DECLARE(counter) \
        volatile uint32_t counter = 0
#else
    #define STATS_INCREMENT(counter) ((void)0)
    #define STATS_DECLARE(counter)
#endif

#endif // ADAPTIVE_ASSERT_H
