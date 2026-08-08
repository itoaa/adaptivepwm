/**
 * @file pwm_cli.h
 * @brief Header file for AdaptivePWM CLI integration
 * 
 * This header defines the interface between the embedded AdaptivePWM system
 * and the command line interface, following the security model of the EJBCA-PKI project.
 */

#ifndef PWM_CLI_H
#define PWM_CLI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// CLI command codes
#define CLI_CMD_STATUS     0x01
#define CLI_CMD_CONFIGURE  0x02
#define CLI_CMD_MONITOR    0x03
#define CLI_CMD_DIAGNOSTICS 0x04

// Configuration parameters
typedef struct {
    float duty_cycle_min;
    float duty_cycle_max;
    float target_efficiency;
    uint32_t sample_rate_ms;
    bool secure_mode_enabled;
} pwm_config_t;

// System status structure
typedef struct {
    float duty_cycle;
    float efficiency;
    float inductance_mH;
    float capacitance_uF;
    float esr_mOhm;
    uint32_t uptime_seconds;
    bool system_ready;
    bool secure_mode_active;
} pwm_status_t;

// Function prototypes

/**
 * @brief Initialize CLI interface
 * @return true if successful, false otherwise
 */
bool cli_init(void);

/**
 * @brief Process incoming CLI command
 * @param command Command code to process
 * @param data Optional data for the command
 * @param data_len Length of data in bytes
 * @return Response code
 */
uint8_t cli_process_command(uint8_t command, const void* data, uint16_t data_len);

/**
 * @brief Get current system status
 * @param status Pointer to status structure to fill
 * @return true if successful, false otherwise
 */
bool cli_get_status(pwm_status_t* status);

/**
 * @brief Configure system parameters
 * @param config Pointer to configuration structure
 * @return true if successful, false otherwise
 */
bool cli_configure(const pwm_config_t* config);

/**
 * @brief Enable or disable secure mode
 * @param enabled true to enable, false to disable
 * @return true if successful, false otherwise
 */
bool cli_set_secure_mode(bool enabled);

/**
 * @brief Validate certificate for secure mode
 * @param cert_data Certificate data
 * @param cert_len Length of certificate data
 * @return true if valid, false otherwise
 */
bool cli_validate_certificate(const uint8_t* cert_data, uint16_t cert_len);

/**
 * @brief Send diagnostic information
 * @param buffer Buffer to fill with diagnostic data
 * @param buffer_size Size of buffer
 * @return Number of bytes written to buffer
 */
uint16_t cli_get_diagnostics(char* buffer, uint16_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // PWM_CLI_H