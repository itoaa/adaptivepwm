/**
 * @file setup_gpio.h
 * @brief First-Time Setup Physical Confirmation GPIO Interface
 * @details Provides GPIO-based physical confirmation for first-time password
 *          setup to prevent remote attackers from setting passwords before
 *          legitimate owner gains physical access.
 * 
 * Security Framework:
 * - CISSP Domain: 5 (Identity and Access Management - IAM)
 * - NIST CSF 2.0: PR.AC-01 (Protect - Access Control)
 * - ISO 27001:2022: A.9.4 (Access control)
 * 
 * Security Assessment Reference:
 * - Finding: ADP-IAM-001 (CVSS 5.3 - MEDIUM)
 * - Task: SEC-031
 * 
 * @version 1.0.0
 * @date 2026-04-16
 */

#ifndef SETUP_GPIO_H
#define SETUP_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// =============================================================================
// VERSION
// =============================================================================
#define SETUP_GPIO_VERSION_MAJOR    1
#define SETUP_GPIO_VERSION_MINOR    0
#define SETUP_GPIO_VERSION_PATCH    0

// =============================================================================
// DATA TYPES
// =============================================================================

/**
 * @brief Setup confirmation result codes
 */
typedef enum {
    SETUP_CONFIRM_OK = 0,           // Physical confirmation received
    SETUP_CONFIRM_TIMEOUT,          // Timeout waiting for confirmation
    SETUP_CONFIRM_NOT_REQUIRED,     // Confirmation not required (disabled or already set)
    SETUP_CONFIRM_INIT_ERROR,         // GPIO initialization failed
    SETUP_CONFIRM_INVALID_MODE        // Invalid confirmation mode
} setup_confirm_result_t;

/**
 * @brief Button/jumper state
 */
typedef enum {
    SETUP_GPIO_STATE_RELEASED = 0,
    SETUP_GPIO_STATE_PRESSED,
    SETUP_GPIO_STATE_UNKNOWN
} setup_gpio_state_t;

// =============================================================================
// API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize GPIO for setup confirmation
 * @details Configures GPIO pins for button/jumper input with pull-up
 * @return true if initialization successful, false otherwise
 */
bool SetupGPIO_Init(void);

/**
 * @brief Check if setup confirmation is required
 * @details Returns true if password not set and confirmation is enabled
 * @return true if physical confirmation is required
 */
bool SetupGPIO_IsConfirmationRequired(void);

/**
 * @brief Wait for physical confirmation (button press or jumper present)
 * @details Blocks until physical confirmation received or timeout
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return Setup confirmation result
 */
setup_confirm_result_t SetupGPIO_WaitForConfirmation(uint32_t timeout_ms);

/**
 * @brief Check if physical confirmation is currently active
 * @details Non-blocking check of button/jumper state
 * @return true if button pressed or jumper installed
 */
bool SetupGPIO_IsConfirmed(void);

/**
 * @brief Get current button state
 * @details Reads GPIO state with debouncing
 * @return Current button state
 */
setup_gpio_state_t SetupGPIO_GetButtonState(void);

/**
 * @brief Get current jumper state
 * @details Reads GPIO state (assumes jumper pulls pin low)
 * @return Current jumper state
 */
setup_gpio_state_t SetupGPIO_GetJumperState(void);

/**
 * @brief Deinitialize GPIO to save power
 * @details Disables GPIO clocks after setup is complete
 * @return true if successful
 */
bool SetupGPIO_Deinit(void);

/**
 * @brief Get human-readable result message
 * @param result Result code
 * @return Human-readable message string
 */
const char* SetupGPIO_GetResultMessage(setup_confirm_result_t result);

/**
 * @brief Get current confirmation mode as string
 * @return Mode description string
 */
const char* SetupGPIO_GetModeString(void);

#endif // SETUP_GPIO_H
