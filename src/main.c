/**
 * @file main.c
 * @brief AdaptivePWM main entry point - Optimized Clock Configuration
 * 
 * Complete implementation with FreeRTOS, HAL layers, and safety systems.
 * Version 2.1.0 with optimized 16MHz HSE clock system.
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
 * @version 2.1.0
 * @date 2026-03-21
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

#include <string.h>
#include <stdio.h>

// Global handles
Adaptive_PWM_t pwm_handle;
Adaptive_ADC_t adc_handle;
Adaptive_UART_t uart_handle;
TaskManager_t task_manager;
ErrorManager_t error_manager;
TempMonitor_t temp_monitor;

// Calculation state
WaveformBuffer_t waveform_buffer;
CalculatedParams_t calc_params;
float current_duty_cycle = 0.5f;

// System state
volatile bool system_running = false;

// Forward declarations
static void SystemClock_Config(void);
static bool Initialize_System(void);

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
    }
    
    system_running = true;
    
    DEBUG_PRINT("AdaptivePWM v%s started", ADAPTIVEPWM_VERSION_STRING);
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
    Error_Init(&error_manager);
    
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
        Adaptive_UART_SendString(&uart_handle, "\r\n");
        Adaptive_UART_SendString(&uart_handle, "AdaptivePWM v" ADAPTIVEPWM_VERSION_STRING "\r\n");
        Adaptive_UART_SendString(&uart_handle, "Clock: 16MHz HSE → 84MHz SYSCLK\r\n");
        Adaptive_UART_SendString(&uart_handle, "System initialized\r\n> ");
    }
    
    if (!Tasks_Init(&task_manager)) {
        DEBUG_PRINT("Tasks_Init failed");
        return false;
    }
    
    if (!CLI_Init()) {
        DEBUG_PRINT("CLI_Init failed");
        return false;
    }
    
    // Start ADC DMA
    if (!Adaptive_ADC_Start_DMA(&adc_handle)) {
        DEBUG_PRINT("ADC_Start_DMA failed");
        return false;
    }
    
    DEBUG_PRINT("System initialization complete");
    return true;
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
            Adaptive_UART_SendString(&uart_handle, "> ");
        }
    }
}
