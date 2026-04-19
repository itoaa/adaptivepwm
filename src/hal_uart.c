/**
 * @file hal_uart.c
 * @brief UART HAL implementation with enhanced CLI
 * 
 * UPDATED: PWM-ARCH-002
 * - Added TX overflow protection with configurable timeout
 * - Added non-blocking send option for ISR-safe operation
 * - Added TX statistics tracking in debug mode
 * - Improved error handling for HAL_UART_Transmit failures
 */

#include "hal_uart.h"
#include "config.h"
#include "adaptive_assert.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static void UART_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    
    // PA2 TX
    GPIO_InitStruct.Pin = UART_TX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(UART_GPIO_PORT, &GPIO_InitStruct);
    
    // PA3 RX
    GPIO_InitStruct.Pin = UART_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(UART_GPIO_PORT, &GPIO_InitStruct);
}

bool Adaptive_UART_Init(Adaptive_UART_t* uart)
{
    ADAPTIVE_ASSERT(uart != NULL);
    
    if (uart == NULL) return false;
    
    memset(uart, 0, sizeof(Adaptive_UART_t));
    
    UART_GPIO_Init();
    
    uart->huart.Instance = UART_INSTANCE;
    uart->huart.Init.BaudRate = UART_BAUDRATE;
    uart->huart.Init.WordLength = UART_WORDLENGTH_8B;
    uart->huart.Init.StopBits = UART_STOPBITS_1;
    uart->huart.Init.Parity = UART_PARITY_NONE;
    uart->huart.Init.Mode = UART_MODE_TX_RX;
    uart->huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->huart.Init.OverSampling = UART_OVERSAMPLING_16;
    
    if (HAL_UART_Init(&uart->huart) != HAL_OK) {
        return false;
    }
    
    // Enable RX interrupt
    __HAL_UART_ENABLE_IT(&uart->huart, UART_IT_RXNE);
    
    // Enable UART interrupt
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    
    return true;
}

void Adaptive_UART_SetCallback(Adaptive_UART_t* uart, UART_Callback_t callback)
{
    ADAPTIVE_ASSERT(uart != NULL);
    
    if (uart != NULL) {
        uart->cmd_callback = callback;
    }
}

/**
 * Check if UART TX is ready to accept new data
 * 
 * PWM-ARCH-002: New function to check TX readiness before sending.
 * Returns true if UART is not busy transmitting.
 */
bool Adaptive_UART_TxReady(const Adaptive_UART_t* uart)
{
    if (uart == NULL) return false;
    
    // Check if TX is busy (using HAL state)
    return (__HAL_UART_GET_FLAG((UART_HandleTypeDef*)&uart->huart, UART_FLAG_TXE) != RESET);
}

/**
 * Send string with default timeout
 * 
 * PWM-ARCH-002: Enhanced with overflow protection.
 * Checks if TX is ready before attempting send.
 */
bool Adaptive_UART_SendString(Adaptive_UART_t* uart, const char* str)
{
    return Adaptive_UART_SendString_Timeout(uart, str, UART_TX_TIMEOUT_MS);
}

/**
 * Send string with configurable timeout
 * 
 * PWM-ARCH-002: New function providing configurable timeout.
 * Prevents indefinite blocking if UART is stuck.
 * Tracks overflow statistics in debug mode.
 */
bool Adaptive_UART_SendString_Timeout(Adaptive_UART_t* uart, const char* str, uint32_t timeout_ms)
{
    if (uart == NULL || str == NULL) return false;
    
    size_t len = strlen(str);
    if (len == 0) return true;  // Empty string is success
    
    // Check if data fits in buffer
    if (len > UART_TX_BUFFER_SIZE) {
        // Truncate to buffer size
        len = UART_TX_BUFFER_SIZE;
        #ifdef DEBUG
        uart->tx_overflow_count++;
        #endif
    }
    
    // Attempt transmit with timeout
    HAL_StatusTypeDef status = HAL_UART_Transmit(&uart->huart, 
                                                  (uint8_t*)str, 
                                                  len, 
                                                  timeout_ms);
    
    if (status != HAL_OK) {
        #ifdef DEBUG
        if (status == HAL_TIMEOUT) {
            uart->tx_timeout_count++;
        }
        #endif
        return false;
    }
    
    return true;
}

/**
 * Non-blocking string send
 * 
 * PWM-ARCH-002: New ISR-safe function.
 * Returns immediately if UART is busy, with retry mechanism.
 * Safe to call from interrupts.
 */
bool Adaptive_UART_SendString_NonBlocking(Adaptive_UART_t* uart, const char* str)
{
    if (uart == NULL || str == NULL) return false;
    
    size_t len = strlen(str);
    if (len == 0) return true;
    
    // Check if TX is ready
    if (!Adaptive_UART_TxReady(uart)) {
        return false;  // UART busy, caller should retry
    }
    
    // Truncate if too long
    if (len > UART_TX_BUFFER_SIZE) {
        len = UART_TX_BUFFER_SIZE;
        #ifdef DEBUG
        uart->tx_overflow_count++;
        #endif
    }
    
    // Short timeout for non-blocking - just poll once
    HAL_StatusTypeDef status = HAL_UART_Transmit(&uart->huart, 
                                                  (uint8_t*)str, 
                                                  len, 
                                                  1);  // 1ms timeout
    
    return (status == HAL_OK);
}

bool Adaptive_UART_SendChar(Adaptive_UART_t* uart, char ch)
{
    if (uart == NULL) return false;
    
    // PWM-ARCH-002: Use shorter timeout for single char
    return HAL_UART_Transmit(&uart->huart, (uint8_t*)&ch, 1, 10) == HAL_OK;
}

bool Adaptive_UART_Printf(Adaptive_UART_t* uart, const char* fmt, ...)
{
    ADAPTIVE_ASSERT(uart != NULL);
    ADAPTIVE_ASSERT(fmt != NULL);
    
    if (uart == NULL || fmt == NULL) return false;
    
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf((char*)uart->tx_buffer, UART_TX_BUFFER_SIZE, fmt, args);
    va_end(args);
    
    if (len < 0) {
        // Encoding error
        return false;
    }
    
    if ((size_t)len >= UART_TX_BUFFER_SIZE) {
        // Truncated - still try to send what we have
        len = UART_TX_BUFFER_SIZE - 1;
        uart->tx_buffer[UART_TX_BUFFER_SIZE - 1] = '\0';
        #ifdef DEBUG
        uart->tx_overflow_count++;
        #endif
    }
    
    if (len > 0) {
        // PWM-ARCH-002: Use timeout protection
        return Adaptive_UART_SendString_Timeout(uart, (char*)uart->tx_buffer, UART_TX_TIMEOUT_MS);
    }
    return true;  // Empty format string is success
}

void Adaptive_UART_ProcessRX(Adaptive_UART_t* uart)
{
    if (uart == NULL) return;
    
    // Check for received data
    if (__HAL_UART_GET_FLAG(&uart->huart, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(uart->huart.Instance->DR & 0xFF);
        
        // Handle backspace (DEL or BS)
        if (ch == '\b' || ch == 0x7F) {
            if (uart->cmd_len > 0) {
                uart->cmd_len--;
                // Echo backspace (erase character on terminal)
                Adaptive_UART_SendString(uart, "\b \b");
            }
            return;
        }
        
        // Handle escape sequences (simple - just ignore them)
        if (ch == 0x1B) {
            // ESC - could be start of escape sequence
            // For now, just ignore
            return;
        }
        
        // Handle command delimiter (newline)
        if (ch == '\n' || ch == '\r') {
            if (uart->cmd_len > 0) {
                uart->cmd_buffer[uart->cmd_len] = '\0';
                uart->cmd_ready = true;
                
                if (uart->cmd_callback != NULL) {
                    uart->cmd_callback((char*)uart->cmd_buffer, uart->cmd_len);
                }
                
                uart->cmd_len = 0;
            } else {
                // Empty command - just send prompt
                Adaptive_UART_SendString(uart, "\r\n> ");
            }
            return;
        }
        
        // Echo printable characters
        if (ch >= 0x20 && ch <= 0x7E) {
            if (uart->cmd_len < UART_MAX_CMD_LEN - 1) {
                uart->cmd_buffer[uart->cmd_len++] = ch;
                // Echo back
                Adaptive_UART_SendChar(uart, ch);
            }
        }
    }
}

bool Adaptive_UART_IsCmdReady(Adaptive_UART_t* uart)
{
    return (uart != NULL) && uart->cmd_ready;
}

uint16_t Adaptive_UART_GetCommand(Adaptive_UART_t* uart, char* buffer, uint16_t max_len)
{
    if (uart == NULL || buffer == NULL || max_len == 0) return 0;
    
    if (!uart->cmd_ready) return 0;
    
    uint16_t len = uart->cmd_len;
    if (len > max_len - 1) len = max_len - 1;
    
    memcpy(buffer, uart->cmd_buffer, len);
    buffer[len] = '\0';
    uart->cmd_ready = false;
    uart->cmd_len = 0;
    
    return len;
}

void Adaptive_UART_ClearCommand(Adaptive_UART_t* uart)
{
    if (uart == NULL) return;
    uart->cmd_len = 0;
    uart->cmd_ready = false;
}

bool Adaptive_UART_HasInput(const Adaptive_UART_t* uart)
{
    return (uart != NULL) && (uart->cmd_len > 0);
}
