/**
 * @file setup_gpio.c
 * @brief First-Time Setup Physical Confirmation Implementation
 * @details GPIO-based physical confirmation for first-time password setup.
 *          Addresses security finding ADP-IAM-001 (SEC-031).
 * 
 * Hardware Configuration:
 * - Primary pin: PA0 (Button with pull-up)
 * - Alternative pin: PA8 (Jumper to GND)
 * 
 * Security Features:
 * - 50ms debounce to prevent false triggers
 * - Minimum 100ms press duration required
 * - 30 second timeout default
 * - Configurable via build options
 * 
 * @version 1.0.0
 * @date 2026-04-16
 */

#include "setup_gpio.h"
#include "cli_auth.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// STATIC VARIABLES
// =============================================================================

static bool gpio_initialized = false;

// Button state tracking for debouncing
static uint32_t button_press_start_ms = 0;
static setup_gpio_state_t last_button_state = SETUP_GPIO_STATE_UNKNOWN;
static setup_gpio_state_t debounced_button_state = SETUP_GPIO_STATE_RELEASED;

// Jumper state tracking
static setup_gpio_state_t last_jumper_state = SETUP_GPIO_STATE_UNKNOWN;
static setup_gpio_state_t debounced_jumper_state = SETUP_GPIO_STATE_RELEASED;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current milliseconds (from HAL_GetTick)
 */
static uint32_t get_current_ms(void)
{
    return HAL_GetTick();
}

/**
 * @brief Read raw GPIO state
 * @return true if pin is low (pressed/connected)
 */
static bool read_gpio_pin(GPIO_TypeDef* port, uint16_t pin)
{
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET);
}

/**
 * @brief Debounce button/jumper input
 * @param current_state Current raw state
 * @param debounced Pointer to debounced state
 * @param last Pointer to last state
 * @param press_start Pointer to press start time
 * @param debounce_ms Debounce time in milliseconds
 * @return Debounced state
 */
static setup_gpio_state_t debounce_input(
    setup_gpio_state_t current_state,
    volatile setup_gpio_state_t* debounced,
    setup_gpio_state_t* last,
    uint32_t* press_start,
    uint32_t debounce_ms)
{
    uint32_t current_time = get_current_ms();
    
    // State changed
    if (current_state != *last) {
        *press_start = current_time;
    }
    
    // State stable for debounce period
    if ((current_time - *press_start) >= debounce_ms) {
        if (current_state != *debounced) {
            *debounced = current_state;
        }
    }
    
    *last = current_state;
    return *debounced;
}

// =============================================================================
// API IMPLEMENTATION
// =============================================================================

bool SetupGPIO_Init(void)
{
    if (gpio_initialized) {
        return true;
    }
    
    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Configure primary confirmation pin (button)
    GPIO_InitStruct.Pin = SETUP_CONFIRM_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  // Pull-up: pressed = low
    HAL_GPIO_Init(SETUP_CONFIRM_GPIO_PORT, &GPIO_InitStruct);
    
    // Configure alternative confirmation pin (jumper) if mode requires it
    #if (SETUP_CONFIRM_MODE == SETUP_MODE_JUMPER) || (SETUP_CONFIRM_MODE == SETUP_MODE_BOTH)
    GPIO_InitStruct.Pin = SETUP_CONFIRM_ALT_GPIO_PIN;
    HAL_GPIO_Init(SETUP_CONFIRM_ALT_GPIO_PORT, &GPIO_InitStruct);
    #endif
    
    // Initialize state tracking
    last_button_state = SETUP_GPIO_STATE_UNKNOWN;
    debounced_button_state = SETUP_GPIO_STATE_RELEASED;
    last_jumper_state = SETUP_GPIO_STATE_UNKNOWN;
    debounced_jumper_state = SETUP_GPIO_STATE_RELEASED;
    button_press_start_ms = 0;
    
    gpio_initialized = true;
    
    #if DEBUG_PRINT_ENABLED
    printf("SetupGPIO: Initialized (mode=%s)\n", SetupGPIO_GetModeString());
    #endif
    
    return true;
}

bool SetupGPIO_IsConfirmationRequired(void)
{
    // Not required if disabled
    #if (SETUP_CONFIRM_ENABLED == 0) || (SETUP_CONFIRM_MODE == SETUP_MODE_NONE)
    return false;
    #endif
    
    // Not required if password already set
    if (CLI_Auth_IsPasswordSet()) {
        return false;
    }
    
    return true;
}

setup_gpio_state_t SetupGPIO_GetButtonState(void)
{
    if (!gpio_initialized) {
        SetupGPIO_Init();
    }
    
    // Read raw state
    bool raw_pressed = read_gpio_pin(SETUP_CONFIRM_GPIO_PORT, SETUP_CONFIRM_GPIO_PIN);
    setup_gpio_state_t current_state = raw_pressed ? SETUP_GPIO_STATE_PRESSED : SETUP_GPIO_STATE_RELEASED;
    
    // Apply debouncing
    return debounce_input(current_state, &debounced_button_state, &last_button_state, 
                          &button_press_start_ms, SETUP_BUTTON_DEBOUNCE_MS);
}

setup_gpio_state_t SetupGPIO_GetJumperState(void)
{
    #if (SETUP_CONFIRM_MODE == SETUP_MODE_BUTTON)
    return SETUP_GPIO_STATE_RELEASED;  // Jumper not used in button-only mode
    #endif
    
    if (!gpio_initialized) {
        SetupGPIO_Init();
    }
    
    // Read raw state
    bool raw_installed = read_gpio_pin(SETUP_CONFIRM_ALT_GPIO_PORT, SETUP_CONFIRM_ALT_GPIO_PIN);
    setup_gpio_state_t current_state = raw_installed ? SETUP_GPIO_STATE_PRESSED : SETUP_GPIO_STATE_RELEASED;
    
    // Jumper doesn't need debounce, but we'll track it anyway
    static uint32_t jumper_press_start = 0;
    return debounce_input(current_state, &debounced_jumper_state, &last_jumper_state,
                          &jumper_press_start, 10);  // Minimal debounce for jumper
}

bool SetupGPIO_IsConfirmed(void)
{
    #if (SETUP_CONFIRM_ENABLED == 0) || (SETUP_CONFIRM_MODE == SETUP_MODE_NONE)
    return true;  // Always confirmed if disabled
    #endif
    
    if (!gpio_initialized) {
        SetupGPIO_Init();
    }
    
    #if SETUP_CONFIRM_MODE == SETUP_MODE_BUTTON
    // Check button only
    return (SetupGPIO_GetButtonState() == SETUP_GPIO_STATE_PRESSED);
    
    #elif SETUP_CONFIRM_MODE == SETUP_MODE_JUMPER
    // Check jumper only
    return (SetupGPIO_GetJumperState() == SETUP_GPIO_STATE_PRESSED);
    
    #elif SETUP_CONFIRM_MODE == SETUP_MODE_BOTH
    // Either button or jumper
    return (SetupGPIO_GetButtonState() == SETUP_GPIO_STATE_PRESSED) ||
           (SetupGPIO_GetJumperState() == SETUP_GPIO_STATE_PRESSED);
    
    #else
    return false;
    #endif
}

setup_confirm_result_t SetupGPIO_WaitForConfirmation(uint32_t timeout_ms)
{
    #if (SETUP_CONFIRM_ENABLED == 0) || (SETUP_CONFIRM_MODE == SETUP_MODE_NONE)
    return SETUP_CONFIRM_NOT_REQUIRED;
    #endif
    
    if (!gpio_initialized) {
        if (!SetupGPIO_Init()) {
            return SETUP_CONFIRM_INIT_ERROR;
        }
    }
    
    // If password already set, confirmation not required
    if (CLI_Auth_IsPasswordSet()) {
        return SETUP_CONFIRM_NOT_REQUIRED;
    }
    
    uint32_t start_time = get_current_ms();
    uint32_t press_start = 0;
    bool press_active = false;
    
    #if DEBUG_PRINT_ENABLED
    printf("SetupGPIO: Waiting for confirmation (timeout=%lu ms)...\n", timeout_ms);
    #endif
    
    while ((get_current_ms() - start_time) < timeout_ms) {
        bool confirmed = false;
        
        #if SETUP_CONFIRM_MODE == SETUP_MODE_BUTTON
        // Button mode: require minimum press duration
        setup_gpio_state_t btn = SetupGPIO_GetButtonState();
        if (btn == SETUP_GPIO_STATE_PRESSED) {
            if (!press_active) {
                press_start = get_current_ms();
                press_active = true;
            } else if ((get_current_ms() - press_start) >= SETUP_BUTTON_PRESS_MIN_MS) {
                confirmed = true;
            }
        } else {
            press_active = false;
        }
        
        #elif SETUP_CONFIRM_MODE == SETUP_MODE_JUMPER
        // Jumper mode: just check if installed
        if (SetupGPIO_GetJumperState() == SETUP_GPIO_STATE_PRESSED) {
            confirmed = true;
        }
        
        #elif SETUP_CONFIRM_MODE == SETUP_MODE_BOTH
        // Both mode: accept either
        setup_gpio_state_t btn = SetupGPIO_GetButtonState();
        if (btn == SETUP_GPIO_STATE_PRESSED) {
            if (!press_active) {
                press_start = get_current_ms();
                press_active = true;
            } else if ((get_current_ms() - press_start) >= SETUP_BUTTON_PRESS_MIN_MS) {
                confirmed = true;
            }
        } else {
            press_active = false;
        }
        
        if (SetupGPIO_GetJumperState() == SETUP_GPIO_STATE_PRESSED) {
            confirmed = true;
        }
        #endif
        
        if (confirmed) {
            #if DEBUG_PRINT_ENABLED
            printf("SetupGPIO: Confirmation received!\n");
            #endif
            return SETUP_CONFIRM_OK;
        }
        
        // Small delay to prevent busy loop
        HAL_Delay(10);
    }
    
    #if DEBUG_PRINT_ENABLED
    printf("SetupGPIO: Timeout waiting for confirmation\n");
    #endif
    
    return SETUP_CONFIRM_TIMEOUT;
}

bool SetupGPIO_Deinit(void)
{
    if (!gpio_initialized) {
        return true;
    }
    
    // Deinitialize GPIO pins
    HAL_GPIO_DeInit(SETUP_CONFIRM_GPIO_PORT, SETUP_CONFIRM_GPIO_PIN);
    
    #if (SETUP_CONFIRM_MODE == SETUP_MODE_JUMPER) || (SETUP_CONFIRM_MODE == SETUP_MODE_BOTH)
    HAL_GPIO_DeInit(SETUP_CONFIRM_ALT_GPIO_PORT, SETUP_CONFIRM_ALT_GPIO_PIN);
    #endif
    
    gpio_initialized = false;
    
    #if DEBUG_PRINT_ENABLED
    printf("SetupGPIO: Deinitialized\n");
    #endif
    
    return true;
}

const char* SetupGPIO_GetResultMessage(setup_confirm_result_t result)
{
    switch (result) {
        case SETUP_CONFIRM_OK:
            return "Physical confirmation received";
        case SETUP_CONFIRM_TIMEOUT:
            return "Timeout waiting for physical confirmation";
        case SETUP_CONFIRM_NOT_REQUIRED:
            return "Physical confirmation not required";
        case SETUP_CONFIRM_INIT_ERROR:
            return "GPIO initialization failed";
        case SETUP_CONFIRM_INVALID_MODE:
            return "Invalid confirmation mode";
        default:
            return "Unknown result";
    }
}

const char* SetupGPIO_GetModeString(void)
{
    #if SETUP_CONFIRM_MODE == SETUP_MODE_NONE
    return "NONE (disabled)";
    #elif SETUP_CONFIRM_MODE == SETUP_MODE_BUTTON
    return "BUTTON (PA0)";
    #elif SETUP_CONFIRM_MODE == SETUP_MODE_JUMPER
    return "JUMPER (PA8)";
    #elif SETUP_CONFIRM_MODE == SETUP_MODE_BOTH
    return "BOTH (PA0 button or PA8 jumper)";
    #else
    return "UNKNOWN";
    #endif
}
