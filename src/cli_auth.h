/**
 * @file cli_auth.h
 * @brief UART CLI Authentication Module
 * @details Implements password/PIN authentication for UART CLI access
 *          with secure storage, lockout protection, and configurable timeouts.
 *          Includes physical confirmation for first-time setup (SEC-031).
 * 
 * Security Framework:
 * - CISSP Domain: 4 (Communication and Network Security) / 5 (IAM)
 * - NIST: PR.AC-01 (Protect - Access Control)
 * - ISO 27001: 8.5 (Secure authentication)
 * 
 * Security Assessment Reference:
 * - Finding: ADP-IAM-001 (CVSS 5.3 - MEDIUM)
 * - Task: SEC-031
 * 
 * @version 1.2.0
 * @date 2026-04-16
 */

#ifndef CLI_AUTH_H
#define CLI_AUTH_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"      // Include config first for CLI_AUTH_HASH_ITERATIONS
#include "hal_uart.h"

// =============================================================================
// VERSION
// =============================================================================
#define CLI_AUTH_VERSION_MAJOR      1
#define CLI_AUTH_VERSION_MINOR      2
#define CLI_AUTH_VERSION_PATCH      0

// =============================================================================
// CONFIGURATION
// =============================================================================

// Authentication settings (configurable via build flags or config.h)
// CLI_AUTH_HASH_ITERATIONS is now defined in config.h (SEC-027)
// Default: 100000 (NIST SP 800-132 compliant, increased from 1000)

#ifndef CLI_AUTH_ENABLED
    #define CLI_AUTH_ENABLED          1
#endif

#ifndef CLI_AUTH_MAX_ATTEMPTS
    #define CLI_AUTH_MAX_ATTEMPTS     3
#endif

#ifndef CLI_AUTH_LOCKOUT_DURATION_S
    #define CLI_AUTH_LOCKOUT_DURATION_S  300  // 5 minutes
#endif

#ifndef CLI_AUTH_PASSWORD_MIN_LEN
    #define CLI_AUTH_PASSWORD_MIN_LEN    4
#endif

#ifndef CLI_AUTH_PASSWORD_MAX_LEN
    #define CLI_AUTH_PASSWORD_MAX_LEN    32
#endif

// Note: CLI_AUTH_HASH_ITERATIONS is defined in config.h
// Compile-time check to ensure minimum security requirements
#if !defined(CLI_AUTH_HASH_ITERATIONS)
    #error "CLI_AUTH_HASH_ITERATIONS must be defined in config.h"
#endif

#if CLI_AUTH_HASH_ITERATIONS < 100000
    #warning "CLI_AUTH_HASH_ITERATIONS below NIST SP 800-132 recommended minimum of 100,000"
#endif

// Salt size for password hashing
#define CLI_AUTH_SALT_SIZE              16
#define CLI_AUTH_HASH_SIZE              32  // SHA-256

// =============================================================================
// DATA TYPES
// =============================================================================

/**
 * @brief Authentication state
 */
typedef enum {
    AUTH_STATE_UNAUTHENTICATED = 0,
    AUTH_STATE_AUTHENTICATED,
    AUTH_STATE_LOCKED_OUT,
    AUTH_STATE_DISABLED
} auth_state_t;

/**
 * @brief Authentication result codes
 */
typedef enum {
    AUTH_OK = 0,
    AUTH_INVALID_PASSWORD,
    AUTH_LOCKED_OUT,
    AUTH_ALREADY_AUTHENTICATED,
    AUTH_NOT_AUTHENTICATED,
    AUTH_HASH_ERROR,
    AUTH_STORAGE_ERROR,
    AUTH_INVALID_LENGTH,
    AUTH_SAME_PASSWORD,
    AUTH_WEAK_PASSWORD,
    AUTH_SETUP_CONFIRMATION_REQUIRED,  // SEC-031: Physical confirmation needed
    AUTH_SETUP_CONFIRMATION_TIMEOUT    // SEC-031: Physical confirmation timeout
} auth_result_t;

/**
 * @brief Authentication context
 */
typedef struct {
    auth_state_t state;
    uint32_t failed_attempts;
    uint32_t lockout_until;      // Timestamp when lockout expires
    uint32_t last_auth_time;     // Timestamp of last successful auth
    uint32_t session_timeout_s;  // Session timeout in seconds
    bool password_set;             // Has password been configured
    bool setup_confirmed;          // SEC-031: Physical confirmation received
} cli_auth_context_t;

/**
 * @brief Stored credentials (in flash)
 */
typedef struct {
    uint8_t salt[CLI_AUTH_SALT_SIZE];
    uint8_t hash[CLI_AUTH_HASH_SIZE];
    uint32_t version;            // Credential format version
    uint32_t timestamp;          // When password was set
    uint32_t crc32;              // Integrity check
} cli_auth_credentials_t;

// =============================================================================
// API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize authentication module
 * @return true if successful, false otherwise
 * @security This function must be called before any auth operations
 */
bool CLI_Auth_Init(void);

/**
 * @brief Check if authentication is enabled
 * @return true if authentication is enabled
 */
bool CLI_Auth_IsEnabled(void);

/**
 * @brief Get current authentication state
 * @return Current authentication state
 */
auth_state_t CLI_Auth_GetState(void);

/**
 * @brief Check if user is authenticated
 * @return true if authenticated
 */
bool CLI_Auth_IsAuthenticated(void);

/**
 * @brief Authenticate with password
 * @param password Password string
 * @return Authentication result code
 * @security Lockout enforced after CLI_AUTH_MAX_ATTEMPTS failed attempts
 * @note For first-time setup, requires physical confirmation (SEC-031)
 */
auth_result_t CLI_Auth_Login(const char* password);

/**
 * @brief Authenticate with password and optional UART for messages
 * @param password Password string
 * @param uart UART handle for status messages (can be NULL)
 * @return Authentication result code
 * @note SEC-031: Shows physical confirmation instructions if needed
 */
auth_result_t CLI_Auth_LoginWithUART(const char* password, Adaptive_UART_t* uart);

/**
 * @brief Logout current session
 * @return true if successful
 */
bool CLI_Auth_Logout(void);

/**
 * @brief Set new password
 * @param old_password Current password (required for verification)
 * @param new_password New password to set
 * @return Authentication result code
 * @security Password must meet minimum length requirements
 * @note For first-time setup, requires physical confirmation (SEC-031)
 */
auth_result_t CLI_Auth_SetPassword(const char* old_password, const char* new_password);

/**
 * @brief Check if password is set
 * @return true if password has been configured
 */
bool CLI_Auth_IsPasswordSet(void);

/**
 * @brief Check if setup confirmation is required
 * @return true if physical confirmation needed for first-time setup
 * @security SEC-031: Prevents remote password setting
 */
bool CLI_Auth_IsSetupConfirmationRequired(void);

/**
 * @brief Request setup confirmation from user
 * @param uart UART handle for status messages
 * @return true if confirmation received, false if timeout/error
 * @security SEC-031: Waits for GPIO button press or jumper
 */
bool CLI_Auth_RequestSetupConfirmation(Adaptive_UART_t* uart);

/**
 * @brief Get remaining lockout time in seconds
 * @return Seconds until lockout expires, 0 if not locked out
 */
uint32_t CLI_Auth_GetLockoutRemaining(void);

/**
 * @brief Get number of failed attempts
 * @return Failed authentication attempts since last success
 */
uint32_t CLI_Auth_GetFailedAttempts(void);

/**
 * @brief Reset failed attempts counter
 * @security Should only be called by authorized admin
 * @return true if successful
 */
bool CLI_Auth_ResetFailedAttempts(void);

/**
 * @brief Check if session has expired
 * @return true if session timeout has elapsed
 */
bool CLI_Auth_IsSessionExpired(void);

/**
 * @brief Refresh session (reset timeout)
 * @return true if successful
 */
bool CLI_Auth_RefreshSession(void);

/**
 * @brief Get human-readable error message
 * @param result Authentication result code
 * @return Error message string
 */
const char* CLI_Auth_GetErrorMessage(auth_result_t result);

/**
 * @brief Set session timeout
 * @param timeout_s Timeout in seconds (0 = no timeout)
 * @return true if successful
 */
bool CLI_Auth_SetSessionTimeout(uint32_t timeout_s);

/**
 * @brief Get session timeout
 * @return Timeout in seconds
 */
uint32_t CLI_Auth_GetSessionTimeout(void);

/**
 * @brief Get remaining session time
 * @return Seconds until session expires, 0 if expired or no timeout
 */
uint32_t CLI_Auth_GetSessionRemaining(void);

// =============================================================================
// COMMAND HANDLERS (for CLI integration)
// =============================================================================

/**
 * @brief Process login command
 * @param uart UART handle for output
 * @param argc Argument count
 * @param argv Argument vector
 * @return true if successful
 */
bool cmd_login(Adaptive_UART_t* uart, int argc, const char* argv[]);

/**
 * @brief Process logout command
 * @param uart UART handle for output
 * @param argc Argument count
 * @param argv Argument vector
 * @return true if successful
 */
bool cmd_logout(Adaptive_UART_t* uart, int argc, const char* argv[]);

/**
 * @brief Process passwd command (change password)
 * @param uart UART handle for output
 * @param argc Argument count
 * @param argv Argument vector
 * @return true if successful
 */
bool cmd_passwd(Adaptive_UART_t* uart, int argc, const char* argv[]);

/**
 * @brief Process authstatus command
 * @param uart UART handle for output
 * @param argc Argument count
 * @param argv Argument vector
 * @return true if successful
 */
bool cmd_authstatus(Adaptive_UART_t* uart, int argc, const char* argv[]);

#endif // CLI_AUTH_H
