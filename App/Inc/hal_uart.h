/**
 * @file hal_uart.h
 * @brief Hardware Abstraction Layer for UART
 * 
 * Implements interrupt-driven UART for CLI communication.
 * Supports command buffering and line-based parsing.
 * 
 * UPDATED: PWM-ARCH-002
 * - Added UART TX overflow protection and non-blocking send options
 * - Added UART_TX_TIMEOUT_MS for configurable timeout
 * - Added Adaptive_UART_SendString_NonBlocking for ISR-safe operation
 * - Added Adaptive_UART_TxReady to check transmit availability
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "config.h"

// UART Configuration
#define UART_INSTANCE       USART2
#define UART_GPIO_PORT      GPIOA
#define UART_TX_PIN         GPIO_PIN_2   // PA2
#define UART_RX_PIN         GPIO_PIN_3   // PA3

// Command buffer
#define UART_MAX_CMD_LEN    128

// UART TX overflow protection configuration (PWM-ARCH-002)
#ifndef UART_TX_TIMEOUT_MS
    #define UART_TX_TIMEOUT_MS      100     // Default timeout for blocking sends
#endif

#ifndef UART_TX_NONBLOCKING_RETRIES
    #define UART_TX_NONBLOCKING_RETRIES  3   // Retry attempts for non-blocking
#endif

typedef void (*UART_Callback_t)(const char* cmd, uint16_t len);

typedef struct {
    UART_HandleTypeDef huart;
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
    uint8_t cmd_buffer[UART_MAX_CMD_LEN];
    uint16_t cmd_len;
    uint16_t rx_head;
    uint16_t rx_tail;
    volatile bool cmd_ready;
    UART_Callback_t cmd_callback;
    // PWM-ARCH-002: Add statistics for overflow tracking
    #ifdef DEBUG
    uint32_t tx_overflow_count;
    uint32_t tx_timeout_count;
    #endif
} Adaptive_UART_t;

// NOTE: Using Adaptive_ prefix to avoid conflicts with STM32 HAL functions

bool Adaptive_UART_Init(Adaptive_UART_t* uart);
void Adaptive_UART_SetCallback(Adaptive_UART_t* uart, UART_Callback_t callback);

// PWM-ARCH-002: Enhanced TX functions with overflow protection
bool Adaptive_UART_SendString(Adaptive_UART_t* uart, const char* str);
bool Adaptive_UART_SendString_Timeout(Adaptive_UART_t* uart, const char* str, uint32_t timeout_ms);
bool Adaptive_UART_SendString_NonBlocking(Adaptive_UART_t* uart, const char* str);
bool Adaptive_UART_TxReady(const Adaptive_UART_t* uart);

bool Adaptive_UART_SendChar(Adaptive_UART_t* uart, char ch);
bool Adaptive_UART_Printf(Adaptive_UART_t* uart, const char* fmt, ...);
void Adaptive_UART_ProcessRX(Adaptive_UART_t* uart);
bool Adaptive_UART_IsCmdReady(Adaptive_UART_t* uart);
uint16_t Adaptive_UART_GetCommand(Adaptive_UART_t* uart, char* buffer, uint16_t max_len);
void Adaptive_UART_ClearCommand(Adaptive_UART_t* uart);
bool Adaptive_UART_HasInput(const Adaptive_UART_t* uart);

#endif // HAL_UART_H
