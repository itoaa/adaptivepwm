/**
 * @file main.c
 * @brief AdaptivePWM main entry point - Enhanced Safety System
 * 
 * Complete implementation with FreeRTOS, HAL layers, safety systems,
 * Enhanced Fault Recovery (PWM-ARCH-004), and UART CLI authentication.
 * 
 * Security Features (PWM-ARCH-004):
 * - Automatic fault recovery
 * - Graceful degradation
 * - Fault history logging to Flash
 * - Predictive maintenance
 * - Multi-level watchdog strategy
 * - CRC validation of critical data
 * - Diagnostic mode
 * 
 * Clock Configuration (16MHz HSE):
 * ================================
 * HSE (16 MHz) → PLL → SYSCLK = 84 MHz
 *   PLLM = 16  → VCO input = 1 MHz
 *   PLLN = 336 → VCO output = 336 MHz
 *   PLLP = 4   → SYSCLK = 84 MHz
 *   PLLQ = 7   → USB = 48 MHz
 * 
 * Bus Clocks:
 *   AHB (HCLK)  = 84 MHz  (max)
 *   APB1 (PCLK1) = 42 MHz  (ADC, UART, TIM2-5)
 *   APB2 (PCLK2) = 84 MHz  (TIM1, ADC)
 * 
 * ADC Clock: 42 MHz (PCLK2/2) - Maximum allowed
 * PWM Clock: 84 MHz (TIM1 on APB2) - Full resolution
 * 
 * @version 2.4.0
 * @date 2026-04-15
 */

#include "stm32f4xx_hal.h"
#include "config.h"
#include "adaptive_assert.h"
#include "hal_pwm.h"
#include "hal_adc.h"
#include "hal_uart.h"
#include "hal_watchdog.h"
#include "param_calc.h"
#include "freertos_tasks.h"
#include "error_handler.h"
#include "temperature_monitor.h"
#include "cli_commands.h"
#include "cli_auth.h"
#include "fault_history.h"
#include "enhanced_safety.h"

#include <string.h>
#include <stdio.h>

// Global handles
Adaptive_PWM_t pwm_handle;
Adaptive_ADC_t adc_handle;
Adaptive_UART_t uart_handle;
TaskManager_t task_manager;
ErrorManager_t error_manager;
TempMonitor_t temp_monitor;
EnhancedSafetyManager_t safety_manager;

// Calculation state
WaveformBuffer_t waveform_buffer;
CalculatedParams_t calc_params;
float current_duty_cycle = 0.5f;

// System state
volatile bool system_running = false;

// Forward declarations
static void SystemClock_Config(void);
static bool Initialize_System(void);
static void Process_Safety_System(void);

/**
 * @brief Main entry point
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    
    // Initialize watchdog early - use WDG_MS_TO_LEVEL macro for clarity
    if (!Adaptive_WDG_Init(WDG_MS_TO_LEVEL(WDG_TIMEOUT_MS))) {
        while (1);  // Halt if watchdog init fails
    }
    
    // Initialize subsystems
    if (!Initialize_System()) {
        Error_Critical(&error_manager, ERR_FREERTOS_ASSERT, "System init failed");
    }
    
    // Log watchdog status
    if (Adaptive_WDG_WasReset()) {
        Error_Report(&error_manager, ERR_WATCHDOG_TIMEOUT, SEVERITY_WARNING,
                     "Watchdog reset occurred", 0);
        
        // Also log to fault history
        FaultHistory_Log(FAULT_TYPE_WATCHDOG_TIMEOUT, FAULT_SEVERITY_WARNING,
                        ERR_WATCHDOG_TIMEOUT, 0);
    }
    
    system_running = true;
    
    DEBUG_PRINT("AdaptivePWM v%s started", ADAPTIVEPWM_VERSION_STRING);
    DEBUG_PRINT("Enhanced Safety System v1.0");
    DEBUG_PRINT("Clock: SYSCLK=%lu MHz, HCLK=%lu MHz", 
                HAL_RCC_GetSysClockFreq()/1000000,
                HAL_RCC_GetHCLKFreq()/1000000);
    
    // Start FreeRTOS scheduler
    Tasks_StartScheduler();
    
    // Should never reach here
    while (1) {
        Adaptive_WDG_Refresh();
    }
}

/**
 * @brief Initialize all system subsystems
 * @return true if successful, false otherwise
 */
static bool Initialize_System(void)
{
    // Initialize error handler (legacy)
    Error_Init(&error_manager);
    
    // Initialize Enhanced Safety System (PWM-ARCH-004)
    if (!EnhancedSafety_Init(&safety_manager)) {
        Error_Report(&error_manager, ERR_INVALID_PARAMS, SEVERITY_ERROR,
                     "Enhanced safety init failed", 0);
        return false;
    }
    
    // Initialize Fault History
    if (!FaultHistory_Init()) {
        Error_Report(&error_manager, ERR_INVALID_PARAMS, SEVERITY_WARNING,
                     "Fault history init failed", 0);
        // Non-fatal - continue without fault history
    }
    
    if (!TempMonitor_Init(&temp_monitor)) {
        Error_Report(&error_manager, ERR_INVALID_PARAMS, SEVERITY_ERROR,
                     "Temp monitor init failed", 0);
        return false;
    }
    
    if (!ParamCalc_Init(&waveform_buffer)) {
        Error_Report(&error_manager, ERR_INVALID_PARAMS, SEVERITY_ERROR,
                     "Param calc init failed", 0);
        return false;
    }
    
    if (!Adaptive_PWM_Init(&pwm_handle)) {
        Error_Report(&error_manager, ERR_PWM_FAULT, SEVERITY_ERROR,
                     "PWM init failed", 0);
        return false;
    }
    
    if (!Adaptive_ADC_Init(&adc_handle)) {
        Error_Report(&error_manager, ERR_ADC_FAILURE, SEVERITY_ERROR,
                     "ADC init failed", 0);
        return false;
    }
    
    if (!Adaptive_UART_Init(&uart_handle)) {
        Error_Report(&error_manager, ERR_CLI_AUTH_FAILURE, SEVERITY_WARNING,
                     "UART init failed", 0);
    } else {
        // Initialize CLI and authentication
        if (!CLI_Init()) {
            Error_Report(&error_manager, ERR_CLI_AUTH_FAILURE, SEVERITY_WARNING,
                         "CLI init failed", 0);
        }
        
        Adaptive_UART_SendString(&uart_handle, "\r\n");
        Adaptive_UART_SendString(&uart_handle, "AdaptivePWM v" ADAPTIVEPWM_VERSION_STRING "\r\n");
        Adaptive_UART_SendString(&uart_handle, "Enhanced Safety System v1.0\r\n");
        Adaptive_UART_SendString(&uart_handle, "Clock: 16MHz HSE → 84MHz SYSCLK\r\n");
        
        // Display authentication status
#if CLI_AUTH_ENABLED
        if (CLI_Auth_IsPasswordSet()) {
            Adaptive_UART_SendString(&uart_handle, "\r\n");
            Adaptive_UART_SendString(&uart_handle, "╔════════════════════════════════════╗\r\n");
            Adaptive_UART_SendString(&uart_handle, "║     AUTHENTICATION REQUIRED        ║\r\n");
            Adaptive_UART_SendString(&uart_handle, "╚════════════════════════════════════╝\r\n");
            Adaptive_UART_SendString(&uart_handle, "\r\n");
            Adaptive_UART_SendString(&uart_handle, "Please login with: login <password>\r\n");
            Adaptive_UART_SendString(&uart_handle, "\r\n");
        } else {
            Adaptive_UART_SendString(&uart_handle, "\r\n");
            Adaptive_UART_SendString(&uart_handle, "╔════════════════════════════════════╗\r\n");
            Adaptive_UART_SendString(&uart_handle, "║   INITIAL SETUP REQUIRED           ║\r\n");
            Adaptive_UART_SendString(&uart_handle, "╚════════════════════════════════════╝\r\n");
            Adaptive_UART_SendString(&uart_handle, "\r\n");
            Adaptive_UART_SendString(&uart_handle, "No password set. First login sets password.\r\n");
            Adaptive_UART_SendString(&uart_handle, "Use: login <new_password>\r\n");
            Adaptive_UART_SendString(&uart_handle, "\r\n");
        }
        Adaptive_UART_SendString(&uart_handle, "\r\n> ");
#else
        Adaptive_UART_SendString(&uart_handle, "System initialized (Auth: DISABLED)\r\n> ");
#endif
    }
    
    if (!Tasks_Init(&task_manager)) {
        DEBUG_PRINT("Tasks_Init failed");
        return false;
    }
    
    // Start ADC DMA
    if (!Adaptive_ADC_Start_DMA(&adc_handle)) {
        DEBUG_PRINT("ADC_Start_DMA failed");
        return false;
    }
    
    DEBUG_PRINT("System initialization complete");
    
    // Log successful startup to fault history
    FaultHistory_Log(FAULT_TYPE_NONE, FAULT_SEVERITY_INFO,
                    0, HAL_RCC_GetSysClockFreq());
    
    return true;
}

/**
 * @brief Process safety system (call periodically from main loop or task)
 * 
 * Handles:
 * - Safety state processing
 * - Watchdog monitoring
 * - Recovery state machine
 * - Diagnostic mode timeout
 */
static void Process_Safety_System(void)
{
    // Process enhanced safety system
    SafetyState_t state = EnhancedSafety_Process(&safety_manager);
    
    // Check for emergency stop
    if (state == SAFETY_STATE_EMERGENCY) {
        // Emergency stop active - ensure PWM is stopped
        if (pwm_handle.is_running) {
            Adaptive_PWM_EmergencyStop(&pwm_handle);
        }
        return;
    }
    
    // Check degradation level and adjust operation
    DegradationLevel_t degradation = EnhancedSafety_GetDegradationLevel(&safety_manager);
    
    if (degradation >= DEGRADATION_SEVERE) {
        // Significant degradation - reduce PWM duty if running
        if (pwm_handle.is_running) {
            float current_duty = Adaptive_PWM_GetDuty(&pwm_handle);
            float max_duty = EnhancedSafety_GetEffectiveDutyLimit(&safety_manager, current_duty);
            
            if (current_duty > max_duty) {
                Adaptive_PWM_SetDuty(&pwm_handle, max_duty);
            }
        }
    }
    
    // Module watchdog checkins
    EnhancedSafety_WatchdogCheckin(&safety_manager, WDG_MODULE_MAIN);
}

/**
 * @brief System Clock Configuration - Optimized for 16MHz HSE
 * 
 * Clock Tree:
 * ===========
 * HSE (16 MHz external crystal)
 *   └── PLL
 *       ├── SYSCLK = 84 MHz (336/4)
 *       └── USB = 48 MHz (336/7)
 * 
 * Bus Clocks:
 *   AHB  = 84 MHz (max performance)
 *   APB1 = 42 MHz (ADC, UART - max 42 MHz)
 *   APB2 = 84 MHz (TIM1 PWM - full speed)
 * 
 * ADC Clock = 42 MHz (APB2/2) - Maximum allowed for 12-bit
 * PWM Clock = 84 MHz (TIM1) - Best resolution at 20kHz
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    // Enable power controller and configure voltage scaling
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    
    // Configure HSE (16 MHz) with PLL
    // Formula: SYSCLK = HSE / PLLM * PLLN / PLLP
    //          84 MHz = 16 / 16 * 336 / 4
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 16;        // VCO input = 16/16 = 1 MHz
    RCC_OscInitStruct.PLL.PLLN = 336;       // VCO output = 336 MHz
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;  // SYSCLK = 336/4 = 84 MHz
    RCC_OscInitStruct.PLL.PLLQ = 7;         // USB = 336/7 = 48 MHz
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Critical(NULL, ERR_INVALID_PARAMS, "Clock config failed");
    }
    
    // Configure bus clocks for optimal performance
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;    // HCLK = 84 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     // PCLK1 = 42 MHz (max for APB1)
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     // PCLK2 = 84 MHz (full speed)
    
    // Flash latency for 84 MHz at 3.3V: 2 wait states
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Critical(NULL, ERR_INVALID_PARAMS, "Clock config failed");
    }
    
    // Verify clock configuration
    SystemCoreClock = HAL_RCC_GetSysClockFreq();
}

// Interrupt handlers
void SysTick_Handler(void)
{
    HAL_IncTick();
    Adaptive_WDG_Refresh();
}

void HardFault_Handler(void)
{
    // Log to fault history before critical error
    FaultHistory_Log(FAULT_TYPE_SOFTWARE_FAULT, FAULT_SEVERITY_FATAL,
                    ERR_FREERTOS_ASSERT, 0);
    
    Error_Critical(&error_manager, ERR_FREERTOS_ASSERT, "Hard fault");
    while (1);
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&adc_handle.hdma);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart_handle.huart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        Adaptive_UART_ProcessRX(&uart_handle);
        if (Adaptive_UART_IsCmdReady(&uart_handle)) {
            char cmd[128];
            Adaptive_UART_GetCommand(&uart_handle, cmd, sizeof(cmd));
            CLI_ProcessCommand(&uart_handle, cmd);
            
            // Check authentication state for prompt
#if CLI_AUTH_ENABLED
            if (CLI_Auth_IsAuthenticated()) {
                uint32_t remaining = CLI_Auth_GetSessionRemaining();
                if (remaining > 0 && remaining < 60) {
                    Adaptive_UART_Printf(&uart_handle, "[timeout: %lus] ", remaining);
                }
            }
#endif
            Adaptive_UART_SendString(&uart_handle, "> ");
        }
    }
}

/**
 * @brief Safety task callback (called from FreeRTOS)
 * 
 * This function should be called periodically (e.g., every 100ms)
 * from a FreeRTOS task to process the safety system.
 */
void Safety_Task_Callback(void)
{
    Process_Safety_System();
}

/**
 * @brief Fault handler integration
 * 
 * Called by error handler to log faults to enhanced system.
 */
void Enhanced_Fault_Handler(uint16_t error_code, uint32_t context)
{
    FaultType_t fault_type = FaultHistory_MapErrorCode(error_code);
    FaultSeverity_t severity = FAULT_SEVERITY_ERROR;
    
    // Map severity from error handler
    switch (error_code) {
        case ERR_OVER_VOLTAGE:
        case ERR_UNDER_VOLTAGE:
        case ERR_OVER_CURRENT:
        case ERR_OVER_TEMP:
            severity = FAULT_SEVERITY_WARNING;
            break;
        case ERR_PWM_FAULT:
        case ERR_ADC_FAILURE:
        case ERR_WATCHDOG_TIMEOUT:
            severity = FAULT_SEVERITY_ERROR;
            break;
        case ERR_INVALID_PARAMS:
        case ERR_FREERTOS_ASSERT:
        case ERR_CLI_AUTH_FAILURE:
            severity = FAULT_SEVERITY_INFO;
            break;
        default:
            severity = FAULT_SEVERITY_ERROR;
            break;
    }
    
    // Log to fault history
    FaultHistory_Log(fault_type, severity, error_code, context);
    
    // Report to enhanced safety system
    EnhancedSafety_ReportFault(&safety_manager, fault_type, severity, error_code, context);
}
