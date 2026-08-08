/**
 * @file features.h
 * @brief Master feature switches — keep "get it working" as the default profile
 *
 * HOW TO USE
 * ----------
 * 1. Edit the defaults in this file, OR
 * 2. Override from the build system, e.g.:
 *      -DFEATURE_CLI_AUTH=1
 *
 * Philosophy:
 * - Core power control, ADC, PWM, basic limits, UART CLI: ON
 * - Higher security / incomplete hardening: OFF until implemented end-to-end
 * - Switch-frequency L/C/ESR estimation: OFF (ADC sampling is not Nyquist-safe at fsw)
 *
 * When you are ready for hardening, flip FEATURE_SECURITY_PROFILE to 1
 * (or enable individual flags) and complete the deferred work in MATURITY.md.
 */

#ifndef ADAPTIVEPWM_FEATURES_H
#define ADAPTIVEPWM_FEATURES_H

/* =============================================================================
 * PROFILES
 * 0 = bring-up / lab (default): functionality first
 * 1 = security-hardened (incomplete — enable only after MATURITY.md items done)
 * ============================================================================= */
#ifndef FEATURE_SECURITY_PROFILE
#define FEATURE_SECURITY_PROFILE            0
#endif

/* =============================================================================
 * SECURITY FEATURES (optional — deferred)
 * Default: all off unless FEATURE_SECURITY_PROFILE=1
 * ============================================================================= */

#ifndef FEATURE_CLI_AUTH
#define FEATURE_CLI_AUTH                    FEATURE_SECURITY_PROFILE
#endif

#ifndef FEATURE_SETUP_CONFIRM
#define FEATURE_SETUP_CONFIRM               FEATURE_SECURITY_PROFILE
#endif

#ifndef FEATURE_FLASH_LOGGER_HMAC
#define FEATURE_FLASH_LOGGER_HMAC           FEATURE_SECURITY_PROFILE
#endif

/* Secure bootloader is a separate binary; this flag is for app/docs awareness */
#ifndef FEATURE_SECURE_BOOT
#define FEATURE_SECURE_BOOT                 FEATURE_SECURITY_PROFILE
#endif

/* STM32F401 has no HW RNG — leave 0 on this MCU */
#ifndef FEATURE_HARDWARE_RNG
#define FEATURE_HARDWARE_RNG                0
#endif

/* Strict compile-time security policies (PBKDF2 floor, etc.) */
#ifndef FEATURE_SECURITY_STRICT
#define FEATURE_SECURITY_STRICT             FEATURE_SECURITY_PROFILE
#endif

/* Production RDP/SWD lock scripts — never auto-enable from firmware */
#ifndef FEATURE_PRODUCTION_LOCKDOWN
#define FEATURE_PRODUCTION_LOCKDOWN         0
#endif

/* =============================================================================
 * CONTROL / MEASUREMENT
 * ============================================================================= */

/*
 * Vout regulation is the primary control path (measurable feedback).
 * Efficiency-based duty hunting is optional and experimental only.
 */
#ifndef FEATURE_VOUT_CONTROL
#define FEATURE_VOUT_CONTROL                1
#endif

/* Circular / estimated efficiency duty loop — OFF by default (see MATURITY) */
#ifndef FEATURE_EFFICIENCY_CONTROL
#define FEATURE_EFFICIENCY_CONTROL          0
#endif

/*
 * Estimate L/C/ESR from switch-frequency ripple.
 * Requires sampling well above PWM_FREQUENCY_HZ (Nyquist). Default OFF.
 */
#ifndef FEATURE_SWITCH_RIPPLE_ESTIMATION
#define FEATURE_SWITCH_RIPPLE_ESTIMATION    0
#endif

/* Dual-stage ADC filtering (noise reduction for mean measurements) */
#ifndef FEATURE_ADC_FILTERING
#define FEATURE_ADC_FILTERING               1
#endif

/* Soft fault recovery: clear after cooldown when limits OK (no infinite hang) */
#ifndef FEATURE_SOFT_FAULT_RECOVERY
#define FEATURE_SOFT_FAULT_RECOVERY         1
#endif

/* =============================================================================
 * COMPATIBILITY ALIASES (used by existing modules)
 * Prefer FEATURE_* names in new code.
 * ============================================================================= */

#ifndef CLI_AUTH_ENABLED
#define CLI_AUTH_ENABLED                    FEATURE_CLI_AUTH
#endif

#ifndef SETUP_CONFIRM_ENABLED
#define SETUP_CONFIRM_ENABLED               FEATURE_SETUP_CONFIRM
#endif

#ifndef FLASH_LOGGER_HMAC_ENABLED
#define FLASH_LOGGER_HMAC_ENABLED           FEATURE_FLASH_LOGGER_HMAC
#endif

#ifndef RNG_ENABLED
#define RNG_ENABLED                         FEATURE_HARDWARE_RNG
#endif

#endif /* ADAPTIVEPWM_FEATURES_H */
