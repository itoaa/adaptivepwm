/**
 * @file performance_profiler.h
 * @brief Performance profiling and cycle-count instrumentation
 * 
 * Provides macros and functions for measuring:
 * - RTOS task timing and jitter
 * - Function execution cycles
 * - ADC-to-PWM latency
 * - CPU load estimation
 * 
 * Uses DWT cycle counter on ARM Cortex-M4
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#ifndef PERFORMANCE_PROFILER_H
#define PERFORMANCE_PROFILER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"

// DWT Cycle Counter registers
#define DWT_CYCCNT          (*(volatile uint32_t*)0xE0001004)
#define DWT_CONTROL         (*(volatile uint32_t*)0xE0001000)
#define SCB_DEMCR           (*(volatile uint32_t*)0xE000EDFC)

// Core clock cycles per microsecond (84MHz = 84 cycles/µs)
#define CYCLES_PER_US       (SystemCoreClock / 1000000UL)

// Enable DWT cycle counter
#define PROFILER_ENABLE()   do { \
    SCB_DEMCR |= (1 << 24); \
    DWT_CONTROL |= 1; \
} while(0)

// Get current cycle count
#define PROFILER_GET_CYCLES()     DWT_CYCCNT

// Convert cycles to microseconds
#define PROFILER_CYCLES_TO_US(cycles)   ((cycles) / CYCLES_PER_US)

// Convert microseconds to cycles
#define PROFILER_US_TO_CYCLES(us)       ((us) * CYCLES_PER_US)

// Maximum profiling entries
#define MAX_PROFILE_SAMPLES     1000
#define MAX_TASK_PROFILERS      4

// Task IDs for profiling
typedef enum {
    TASK_ID_MEASURE = 0,
    TASK_ID_CONTROL,
    TASK_ID_SAFETY,
    TASK_ID_COUNT
} TaskProfilerID_t;

// Task timing statistics
typedef struct {
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint32_t avg_cycles;
    uint32_t jitter_cycles;
    uint32_t last_cycles;
    uint32_t expected_cycles;
    uint32_t sample_count;
    uint32_t violations;
} TaskTimingStats_t;

// ADC-to-PWM latency tracking
typedef struct {
    uint32_t adc_timestamp;
    uint32_t pwm_timestamp;
    uint32_t latency_cycles;
    uint32_t min_latency;
    uint32_t max_latency;
    uint32_t avg_latency;
    uint32_t sample_count;
    uint32_t violations;
} ADCToPWMLatency_t;

// Global profiler state
typedef struct {
    TaskTimingStats_t task_stats[MAX_TASK_PROFILERS];
    ADCToPWMLatency_t latency_tracker;
    uint32_t cpu_load_percent;
    uint32_t total_cycles_idle;
    uint32_t total_cycles_busy;
    uint32_t last_update_ms;
    bool initialized;
} PerformanceProfiler_t;

// Cycle buffer for jitter calculation
typedef struct {
    uint32_t cycles[MAX_PROFILE_SAMPLES];
    uint16_t write_index;
    uint16_t count;
} CycleBuffer_t;

// Static instances
extern PerformanceProfiler_t g_profiler;
extern CycleBuffer_t g_cycle_buffers[MAX_TASK_PROFILERS];

// =============================================================================
// PROFILER API
// =============================================================================

/**
 * @brief Initialize performance profiler
 * @param profiler Pointer to profiler structure
 * @return true on success, false on failure
 */
bool Profiler_Init(PerformanceProfiler_t* profiler);

/**
 * @brief Start timing measurement for a task
 * @param task_id Task identifier
 * @return Current cycle count
 */
static FORCE_INLINE uint32_t Profiler_StartTiming(TaskProfilerID_t task_id) {
    (void)task_id;  // Used for future extensions
    return PROFILER_GET_CYCLES();
}

/**
 * @brief End timing measurement and update statistics
 * @param task_id Task identifier
 * @param start_cycles Cycle count from Profiler_StartTiming
 */
void Profiler_EndTiming(TaskProfilerID_t task_id, uint32_t start_cycles);

/**
 * @brief Mark ADC conversion complete (for latency tracking)
 * @param profiler Pointer to profiler
 */
void Profiler_MarkADCComplete(PerformanceProfiler_t* profiler);

/**
 * @brief Mark PWM update (for latency tracking)
 * @param profiler Pointer to profiler
 */
void Profiler_MarkPWMUpdate(PerformanceProfiler_t* profiler);

/**
 * @brief Update CPU load calculation
 * @param profiler Pointer to profiler
 * @param is_idle true if system was idle, false if busy
 */
void Profiler_UpdateCPULoad(PerformanceProfiler_t* profiler, bool is_idle);

/**
 * @brief Get formatted performance report
 * @param profiler Pointer to profiler
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written
 */
uint32_t Profiler_GetReport(PerformanceProfiler_t* profiler, char* buffer, uint32_t buffer_size);

/**
 * @brief Reset all statistics
 * @param profiler Pointer to profiler
 */
void Profiler_Reset(PerformanceProfiler_t* profiler);

/**
 * @brief Calculate jitter from recent samples
 * @param task_id Task identifier
 * @return Jitter in cycles (max deviation from expected)
 */
uint32_t Profiler_CalculateJitter(TaskProfilerID_t task_id);

// =============================================================================
// CONVENIENCE MACROS
// =============================================================================

// Profile a code block
#define PROFILE_BLOCK(task_id, code) do { \
    uint32_t _start = Profiler_StartTiming(task_id); \
    code; \
    Profiler_EndTiming(task_id, _start); \
} while(0)

// Profile a function call
#define PROFILE_FUNC(task_id, func) PROFILE_BLOCK(task_id, func)

// Mark ADC and PWM for latency tracking
#define PROFILE_ADC_COMPLETE()      Profiler_MarkADCComplete(&g_profiler)
#define PROFILE_PWM_UPDATE()        Profiler_MarkPWMUpdate(&g_profiler)

// Check if jitter is within limits
#define JITTER_CHECK_US(task_id, limit_us) \
    (Profiler_CalculateJitter(task_id) <= PROFILER_US_TO_CYCLES(limit_us))

// =============================================================================
// MEMORY USAGE TRACKING
// =============================================================================

#ifdef USE_FREERTOS
    #include "FreeRTOS.h"
    
    /**
     * @brief Get total heap usage percentage
     * @return Usage percentage (0-100)
     */
    static FORCE_INLINE uint32_t Profiler_GetHeapUsagePercent(void) {
        size_t total = configTOTAL_HEAP_SIZE;
        size_t free = xPortGetFreeHeapSize();
        return (uint32_t)(((total - free) * 100) / total);
    }
    
    /**
     * @brief Get stack high water mark for a task
     * @param task_handle Task handle
     * @return Minimum free stack in words
     */
    static FORCE_INLINE uint32_t Profiler_GetStackHighWaterMark(TaskHandle_t task_handle) {
        return (uint32_t)uxTaskGetStackHighWaterMark(task_handle);
    }
#else
    #define Profiler_GetHeapUsagePercent()  0
    #define Profiler_GetStackHighWaterMark(x) 0
#endif

// =============================================================================
// COMPILE-TIME ASSERTIONS
// =============================================================================

// Ensure cycle counter resolution is sufficient
#if (CYCLES_PER_US < 10)
    #warning "Low clock frequency may limit profiling accuracy"
#endif

// Validate timing requirements
#if (MAX_JITTER_US < 1)
    #error "MAX_JITTER_US must be at least 1µs"
#endif

#endif // PERFORMANCE_PROFILER_H
