/**
 * @file freertos_tasks.c
 * @brief Runtime loops: FreeRTOS tasks OR bare-metal cooperative superloop
 *
 * Default build (no USE_FREERTOS): Tasks_StartScheduler() runs a real
 * measure / control / safety / CLI superloop. FreeRTOS path reuses the same
 * Loop_* helpers when enabled later.
 */

#include "freertos_tasks.h"
#include "hal_pwm.h"
#include "hal_adc.h"
#include "hal_uart.h"
#include "hal_watchdog.h"
#include "param_calc.h"
#include "efficiency_calc.h"
#include "cli_commands.h"
#include "error_handler.h"
#include "config.h"
#include "adaptive_assert.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

extern Adaptive_PWM_t pwm_handle;
extern Adaptive_ADC_t adc_handle;
extern Adaptive_UART_t uart_handle;
extern WaveformBuffer_t waveform_buffer;
extern CalculatedParams_t calc_params;
extern float current_duty_cycle;
extern ErrorManager_t error_manager;

static EfficiencyCalcContext_t eff_calc_ctx;
#if FEATURE_EFFICIENCY_CONTROL
static PowerMeasurement_t power_meas;
#endif

/* Shared control / safety state */
static PID_Controller_t vout_pid;
static float vout_setpoint_v = VOUT_SETPOINT_DEFAULT_V;
static bool control_enabled = true;
static bool safety_fault_latched = false;
static uint32_t last_fault_code = 0;
static uint32_t fault_time_ms = 0;
static uint32_t pwm_run_since_ms = 0;
static bool params_ready = false;

/* Optional: non-blocking monitor state (CLI runs in loop, not IRQ) */
static bool monitor_active = false;
static int monitor_remaining_s = 0;
static int monitor_t = 0;
static uint32_t monitor_next_ms = 0;
static Adaptive_UART_t* monitor_uart = NULL;

#ifdef USE_FREERTOS
static void Task_Measurement(void* pvParameters);
static void Task_Control(void* pvParameters);
static void Task_Safety(void* pvParameters);
static void Task_CLI(void* pvParameters);
#endif

static void Loop_Measure(TaskManager_t* manager);
static void Loop_Control(TaskManager_t* manager);
static void Loop_Safety(TaskManager_t* manager);
static void Loop_CLI(TaskManager_t* manager);

/* -------------------------------------------------------------------------- */
/* Public API used by CLI                                                      */
/* -------------------------------------------------------------------------- */

float Tasks_GetVoutSetpoint(void)
{
    return vout_setpoint_v;
}

void Tasks_SetVoutSetpoint(float v)
{
    if (v < 0.0f) {
        v = 0.0f;
    }
    if (v > VOLTAGE_MAX_V) {
        v = VOLTAGE_MAX_V;
    }
    vout_setpoint_v = v;
    PID_Reset(&vout_pid);
}

bool Tasks_IsSafetyFault(void)
{
    return safety_fault_latched;
}

uint32_t Tasks_GetLastFaultCode(void)
{
    return last_fault_code;
}

void Tasks_ClearSafetyFault(void)
{
    safety_fault_latched = false;
    last_fault_code = 0;
    fault_time_ms = 0;
    PID_Reset(&vout_pid);
}

void Tasks_StartMonitor(Adaptive_UART_t* uart, int duration_s)
{
    if (uart == NULL) {
        return;
    }
    if (duration_s < 1) {
        duration_s = 1;
    }
    if (duration_s > 60) {
        duration_s = 60;
    }
    monitor_uart = uart;
    monitor_remaining_s = duration_s;
    monitor_t = 0;
    monitor_active = true;
    monitor_next_ms = HAL_GetTick();
    Adaptive_UART_Printf(uart, "t_s,Vin,Vout,I,Temp,Duty%%,Fault\r\n");
}

bool Tasks_MonitorActive(void)
{
    return monitor_active;
}

/* -------------------------------------------------------------------------- */
/* Init / scheduler                                                            */
/* -------------------------------------------------------------------------- */

bool Tasks_Init(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);

    if (manager == NULL) {
        return false;
    }

    memset(manager, 0, sizeof(TaskManager_t));

    if (!EfficiencyCalc_Init(&eff_calc_ctx, EFF_MODE_MEASUREMENT, TOPOLOGY_BUCK)) {
        return false;
    }

    PID_Init(&vout_pid, DUTY_KP, DUTY_KI, DUTY_KD,
             PWM_SOFT_MIN_DUTY, PWM_SOFT_MAX_DUTY);
    PID_SetSetpointWeight(&vout_pid, PID_SETPOINT_WEIGHT);
    PID_SetDerivativeFilter(&vout_pid, PID_DERIVATIVE_FILTER);
    vout_setpoint_v = VOUT_SETPOINT_DEFAULT_V;

    manager->adc_ready_sem = xSemaphoreCreateBinary();
    manager->pwm_ready_sem = xSemaphoreCreateBinary();
    manager->params_ready_sem = xSemaphoreCreateBinary();

    if (manager->adc_ready_sem == NULL ||
        manager->pwm_ready_sem == NULL ||
        manager->params_ready_sem == NULL) {
        return false;
    }

    manager->duty_queue = xQueueCreate(4, sizeof(float));
    manager->error_queue = xQueueCreate(4, sizeof(uint32_t));

    if (manager->duty_queue == NULL || manager->error_queue == NULL) {
        return false;
    }

#ifdef USE_FREERTOS
    if (xTaskCreate(Task_Safety, "Safety", STACK_SIZE_SAFETY, manager,
                    TASK_PRIORITY_SAFETY, &manager->safety_handle) != pdPASS) {
        return false;
    }
    if (xTaskCreate(Task_Measurement, "Measure", STACK_SIZE_MEASURE, manager,
                    TASK_PRIORITY_MEASURE, &manager->measure_handle) != pdPASS) {
        return false;
    }
    if (xTaskCreate(Task_Control, "Control", STACK_SIZE_CONTROL, manager,
                    TASK_PRIORITY_CONTROL, &manager->control_handle) != pdPASS) {
        return false;
    }
    if (xTaskCreate(Task_CLI, "CLI", STACK_SIZE_CLI, manager,
                    TASK_PRIORITY_CLI, &manager->cli_handle) != pdPASS) {
        return false;
    }
#endif

    manager->system_state = TASK_STATE_RUNNING;
    manager->active_task_count = 4;
    return true;
}

void Tasks_StartScheduler(void)
{
#ifdef USE_FREERTOS
    vTaskStartScheduler();
#else
    /*
     * Bare-metal cooperative superloop — default bring-up path (no FreeRTOS).
     */
    extern TaskManager_t task_manager;
    TaskManager_t* manager = &task_manager;

    uint32_t last_measure_ms = 0;
    uint32_t last_control_ms = 0;
    uint32_t last_safety_ms = 0;
    uint32_t last_cli_ms = 0;

    while (1) {
        uint32_t now = HAL_GetTick();

        Adaptive_WDG_Refresh();

        if ((now - last_measure_ms) >= (1000U / TASK_FREQ_MEASURE)) {
            Loop_Measure(manager);
            last_measure_ms = now;
        }
        if ((now - last_control_ms) >= (1000U / TASK_FREQ_CONTROL)) {
            Loop_Control(manager);
            last_control_ms = now;
        }
        if ((now - last_safety_ms) >= (1000U / TASK_FREQ_SAFETY)) {
            Loop_Safety(manager);
            last_safety_ms = now;
        }
        if ((now - last_cli_ms) >= (1000U / TASK_FREQ_CLI)) {
            Loop_CLI(manager);
            last_cli_ms = now;
        }
    }
#endif
}

void Tasks_SuspendControl(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    if (manager == NULL) {
        return;
    }
    control_enabled = false;
#ifdef USE_FREERTOS
    vTaskSuspend(manager->control_handle);
    vTaskSuspend(manager->measure_handle);
#endif
    if (manager->active_task_count >= 2U) {
        manager->active_task_count -= 2U;
    }
}

void Tasks_ResumeControl(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    if (manager == NULL) {
        return;
    }
    control_enabled = true;
#ifdef USE_FREERTOS
    vTaskResume(manager->control_handle);
    vTaskResume(manager->measure_handle);
#endif
    manager->active_task_count += 2U;
}

void Tasks_TriggerSafety(TaskManager_t* manager, uint32_t error_code)
{
    if (manager == NULL) {
        return;
    }
#ifdef USE_FREERTOS
    xQueueSend(manager->error_queue, &error_code, 0);
#endif
    manager->system_state = TASK_STATE_ERROR;
    last_fault_code = error_code;
    safety_fault_latched = true;
    fault_time_ms = HAL_GetTick();
    Adaptive_PWM_EmergencyStop(&pwm_handle);
}

uint32_t Tasks_GetStats(const TaskManager_t* manager, char* buffer, uint32_t buffer_size)
{
    if (manager == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    int written = snprintf(buffer, buffer_size,
        "=== AdaptivePWM Runtime ===\r\n"
        "Mode: %s\r\n"
        "State: %d  Fault: %s (code=0x%lx)\r\n"
        "Vout SP: %.2f V  Duty: %.1f%%\r\n"
        "Control: Vout=%d Eff=%d\r\n",
#ifdef USE_FREERTOS
        "FreeRTOS",
#else
        "bare-metal superloop",
#endif
        (int)manager->system_state,
        safety_fault_latched ? "LATCHED" : "OK",
        (unsigned long)last_fault_code,
        (double)vout_setpoint_v,
        (double)(current_duty_cycle * 100.0f),
        FEATURE_VOUT_CONTROL,
        FEATURE_EFFICIENCY_CONTROL);

    if (written < 0) {
        return 0;
    }
    return (uint32_t)written;
}

/* -------------------------------------------------------------------------- */
/* Shared loop bodies                                                          */
/* -------------------------------------------------------------------------- */

static void Loop_Measure(TaskManager_t* manager)
{
    (void)manager;
    ADC_Measurement_t meas;

    /* DMA complete flag from ISR → process buffer in thread context */
    if (Adaptive_ADC_CheckDMAComplete()) {
        Adaptive_ADC_ProcessBuffer(&adc_handle);
    }

    if (Adaptive_ADC_IsReady(&adc_handle) || adc_handle.current.valid) {
        if (Adaptive_ADC_GetAveraged(&adc_handle, &meas) ||
            Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
            ParamCalc_AddSample(&waveform_buffer, &meas);
            ParamCalc_CalculateAll(&waveform_buffer, current_duty_cycle,
                                   (float)PWM_FREQUENCY_HZ, &calc_params);
            params_ready = true;
#ifdef USE_FREERTOS
            if (manager != NULL && manager->params_ready_sem != NULL) {
                xSemaphoreGive(manager->params_ready_sem);
            }
#endif
        }
    }

    if (pwm_handle.is_running && pwm_run_since_ms == 0U) {
        pwm_run_since_ms = HAL_GetTick();
    }
    if (!pwm_handle.is_running) {
        pwm_run_since_ms = 0U;
    }
}

static void Loop_Control(TaskManager_t* manager)
{
    (void)manager;
    ADC_Measurement_t meas;
    const float dt = 1.0f / (float)TASK_FREQ_CONTROL;

    if (!control_enabled || safety_fault_latched) {
        return;
    }
    if (!params_ready) {
        return;
    }
    if (!pwm_handle.is_running) {
        return;
    }

    if (!Adaptive_ADC_GetAveraged(&adc_handle, &meas) &&
        !Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
        return;
    }

    float new_duty = current_duty_cycle;

#if FEATURE_VOUT_CONTROL
    /* Primary path: regulate measured Vout to setpoint */
    new_duty = PID_Compute(&vout_pid, vout_setpoint_v, meas.vout, dt);
#elif FEATURE_EFFICIENCY_CONTROL
    {
        float efficiency = EfficiencyCalc_FromMeasurements(
            &eff_calc_ctx, &meas, &power_meas);
        if (power_meas.valid) {
            efficiency = EfficiencyCalc_GetFiltered(&eff_calc_ctx, efficiency);
            float error = TARGET_EFFICIENCY - efficiency;
            new_duty = current_duty_cycle + error * DUTY_KP;
        }
    }
#else
    /* Open-loop: keep last duty */
    (void)meas;
    (void)eff_calc_ctx;
#endif

    if (new_duty < PWM_SOFT_MIN_DUTY) {
        new_duty = PWM_SOFT_MIN_DUTY;
    }
    if (new_duty > PWM_SOFT_MAX_DUTY) {
        new_duty = PWM_SOFT_MAX_DUTY;
    }

    if (fabsf(new_duty - current_duty_cycle) > PWM_DUTY_HYSTERESIS) {
        if (Adaptive_PWM_SetDuty(&pwm_handle, new_duty)) {
            current_duty_cycle = new_duty;
        }
    }
}

static void Loop_Safety(TaskManager_t* manager)
{
    ADC_Measurement_t meas;
    bool fault = false;
    uint32_t error_code = 0;
    uint32_t now = HAL_GetTick();

    if (!Adaptive_ADC_GetAveraged(&adc_handle, &meas) &&
        !Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
        /* No data yet — do not trip */
        return;
    }

    /* Over-temperature */
    if (meas.temperature > TEMP_SHUTDOWN_C) {
        fault = true;
        error_code = 0x01;
    }

    /* Over-current */
    if (fabsf(meas.current) > CURRENT_MAX_A) {
        fault = true;
        error_code = 0x02;
    }

    /* Over-voltage (input or output) */
    if (meas.vin > VOLTAGE_MAX_V || meas.vout > VOLTAGE_MAX_V) {
        fault = true;
        error_code = 0x03;
    }

    /*
     * VIN UV optional (often unwired on Nucleo bring-up).
     * Vout UV only after PWM has been running for SAFETY_VOUT_UV_ENABLE_MS.
     */
#if SAFETY_VIN_UV_ENABLE
    if (meas.vin < VOLTAGE_MIN_V) {
        fault = true;
        error_code = 0x04;
    }
#endif
    if (pwm_handle.is_running && pwm_run_since_ms != 0U &&
        (now - pwm_run_since_ms) >= SAFETY_VOUT_UV_ENABLE_MS) {
        if (meas.vout < VOLTAGE_MIN_V) {
            fault = true;
            error_code = 0x04;
        }
    }

#if FEATURE_SWITCH_RIPPLE_ESTIMATION
    if (calc_params.valid &&
        calc_params.ripple_current > CURRENT_MAX_A * 0.5f) {
        fault = true;
        error_code = 0x05;
    }
#endif

    if (fault) {
        Adaptive_PWM_EmergencyStop(&pwm_handle);
        if (manager != NULL) {
            manager->system_state = TASK_STATE_ERROR;
        }
        if (!safety_fault_latched) {
            last_fault_code = error_code;
            fault_time_ms = now;
            Error_Report(&error_manager, ERR_PWM_FAULT, SEVERITY_ERROR,
                         "Safety fault latched", error_code);
        }
        safety_fault_latched = true;
        return;
    }

#if FEATURE_SOFT_FAULT_RECOVERY
    /* Clear latch after cooldown when measurements are healthy (with hysteresis) */
    if (safety_fault_latched &&
        (now - fault_time_ms) >= SAFETY_FAULT_COOLDOWN_MS) {
        bool healthy =
            (meas.vin >= (VOLTAGE_MIN_V + VOLTAGE_UV_HYSTERESIS_V)) &&
            (meas.vin <= VOLTAGE_MAX_V) &&
            (meas.vout <= VOLTAGE_MAX_V) &&
            (fabsf(meas.current) < CURRENT_WARNING_A) &&
            (meas.temperature < (TEMP_SHUTDOWN_C - TEMP_HYSTERESIS_C));

        if (healthy) {
            safety_fault_latched = false;
            last_fault_code = 0;
            if (manager != NULL) {
                manager->system_state = TASK_STATE_RUNNING;
            }
            PID_Reset(&vout_pid);
            /* PWM stays off until operator issues `pwm start` */
        }
    }
#endif
}

static void Loop_CLI(TaskManager_t* manager)
{
    (void)manager;
    char cmd[UART_MAX_CMD_LEN];

    /* Process complete lines queued by UART RX ISR (no heavy work in IRQ) */
    if (Adaptive_UART_IsCmdReady(&uart_handle)) {
        if (Adaptive_UART_GetCommand(&uart_handle, cmd, sizeof(cmd)) > 0) {
            CLI_ProcessCommand(&uart_handle, cmd);
            Adaptive_UART_SendString(&uart_handle, "> ");
        }
    }

    /* Non-blocking monitor: one CSV line per second while active */
    if (monitor_active && monitor_uart != NULL) {
        uint32_t now = HAL_GetTick();
        if (now >= monitor_next_ms) {
            ADC_Measurement_t meas;
            float duty = Adaptive_PWM_GetDuty(&pwm_handle) * 100.0f;
            if (Adaptive_ADC_GetAveraged(&adc_handle, &meas) ||
                Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
                Adaptive_UART_Printf(monitor_uart,
                    "%d,%.3f,%.3f,%.3f,%.1f,%.2f,%s\r\n",
                    monitor_t, (double)meas.vin, (double)meas.vout, (double)meas.current,
                    (double)meas.temperature, (double)duty,
                    safety_fault_latched ? "FAULT" : "OK");
            } else {
                Adaptive_UART_Printf(monitor_uart, "%d,NA,NA,NA,NA,%.2f,%s\r\n",
                    monitor_t, (double)duty, safety_fault_latched ? "FAULT" : "OK");
            }
            monitor_t++;
            monitor_remaining_s--;
            monitor_next_ms = now + 1000U;
            if (monitor_remaining_s <= 0) {
                monitor_active = false;
                monitor_t = 0;
                Adaptive_UART_Printf(monitor_uart, "monitor done\r\n> ");
                monitor_uart = NULL;
            }
        }
    }
}

#ifdef USE_FREERTOS
static void Task_Measurement(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        Loop_Measure(manager);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / TASK_FREQ_MEASURE));
    }
}

static void Task_Control(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        Loop_Control(manager);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / TASK_FREQ_CONTROL));
    }
}

static void Task_Safety(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        Loop_Safety(manager);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / TASK_FREQ_SAFETY));
    }
}

static void Task_CLI(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        Loop_CLI(manager);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / TASK_FREQ_CLI));
    }
}
#endif /* USE_FREERTOS */
