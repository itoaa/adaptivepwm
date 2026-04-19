/**
 * @file performance_profiler.c
 * @brief Implementation of performance profiling and cycle-count instrumentation
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#include "performance_profiler.h"

// Global profiler instances
PerformanceProfiler_t g_profiler;
CycleBuffer_t g_cycle_buffers[MAX_TASK_PROFILERS];

// Expected cycle counts for each task (calculated at compile time)
static const uint32_t EXPECTED_CYCLES[MAX_TASK_PROFILERS] = {
    [TASK_ID_MEASURE] = PROFILER_US_TO_CYCLES(100),      // 100µs for measurement task
    [TASK_ID_CONTROL] = PROFILER_US_TO_CYCLES(500),      // 500µs for control task
    [TASK_ID_SAFETY] = PROFILER_US_TO_CYCLES(200),       // 200µs for safety task
};

bool Profiler_Init(PerformanceProfiler_t* profiler)
{
    if (profiler == NULL) return false;
    
    memset(profiler, 0, sizeof(PerformanceProfiler_t));
    memset(g_cycle_buffers, 0, sizeof(g_cycle_buffers));
    
    // Initialize each task stats with expected values
    for (int i = 0; i < MAX_TASK_PROFILERS; i++) {
        profiler->task_stats[i].min_cycles = UINT32_MAX;
        profiler->task_stats[i].max_cycles = 0;
        profiler->task_stats[i].avg_cycles = EXPECTED_CYCLES[i];
        profiler->task_stats[i].expected_cycles = EXPECTED_CYCLES[i];
    }
    
    // Initialize latency tracker
    profiler->latency_tracker.min_latency = UINT32_MAX;
    profiler->latency_tracker.max_latency = 0;
    
    // Enable DWT cycle counter
    PROFILER_ENABLE();
    
    profiler->initialized = true;
    return true;
}

void Profiler_EndTiming(TaskProfilerID_t task_id, uint32_t start_cycles)
{
    if (task_id >= TASK_ID_COUNT) return;
    
    uint32_t end_cycles = PROFILER_GET_CYCLES();
    uint32_t elapsed_cycles;
    
    // Handle cycle counter wraparound
    if (end_cycles >= start_cycles) {
        elapsed_cycles = end_cycles - start_cycles;
    } else {
        elapsed_cycles = (UINT32_MAX - start_cycles) + end_cycles + 1;
    }
    
    TaskTimingStats_t* stats = &g_profiler.task_stats[task_id];
    CycleBuffer_t* buffer = &g_cycle_buffers[task_id];
    
    // Update statistics
    if (elapsed_cycles < stats->min_cycles) {
        stats->min_cycles = elapsed_cycles;
    }
    if (elapsed_cycles > stats->max_cycles) {
        stats->max_cycles = elapsed_cycles;
    }
    
    // Update running average (exponential moving average)
    if (stats->sample_count == 0) {
        stats->avg_cycles = elapsed_cycles;
    } else {
        stats->avg_cycles = (stats->avg_cycles * 15 + elapsed_cycles) / 16;
    }
    
    stats->last_cycles = elapsed_cycles;
    stats->sample_count++;
    
    // Check if timing violation occurred
    uint32_t jitter = 0;
    if (elapsed_cycles > stats->expected_cycles) {
        jitter = elapsed_cycles - stats->expected_cycles;
    }
    
    if (PROFILER_CYCLES_TO_US(jitter) > MAX_JITTER_US) {
        stats->violations++;
    }
    
    // Store in circular buffer for jitter calculation
    buffer->cycles[buffer->write_index] = elapsed_cycles;
    buffer->write_index++;
    if (buffer->write_index >= MAX_PROFILE_SAMPLES) {
        buffer->write_index = 0;
    }
    if (buffer->count < MAX_PROFILE_SAMPLES) {
        buffer->count++;
    }
}

uint32_t Profiler_CalculateJitter(TaskProfilerID_t task_id)
{
    if (task_id >= TASK_ID_COUNT) return 0;
    
    CycleBuffer_t* buffer = &g_cycle_buffers[task_id];
    TaskTimingStats_t* stats = &g_profiler.task_stats[task_id];
    
    if (buffer->count == 0) return 0;
    
    // Calculate jitter as max deviation from expected
    uint32_t max_deviation = 0;
    uint16_t count = buffer->count;
    
    for (uint16_t i = 0; i < count; i++) {
        uint32_t diff;
        if (buffer->cycles[i] > stats->expected_cycles) {
            diff = buffer->cycles[i] - stats->expected_cycles;
        } else {
            diff = stats->expected_cycles - buffer->cycles[i];
        }
        if (diff > max_deviation) {
            max_deviation = diff;
        }
    }
    
    return max_deviation;
}

void Profiler_MarkADCComplete(PerformanceProfiler_t* profiler)
{
    if (profiler == NULL || !profiler->initialized) return;
    
    profiler->latency_tracker.adc_timestamp = PROFILER_GET_CYCLES();
}

void Profiler_MarkPWMUpdate(PerformanceProfiler_t* profiler)
{
    if (profiler == NULL || !profiler->initialized) return;
    
    ADCToPWMLatency_t* lat = &profiler->latency_tracker;
    
    // Only calculate if ADC was recently completed
    if (lat->adc_timestamp == 0) return;
    
    lat->pwm_timestamp = PROFILER_GET_CYCLES();
    
    // Calculate latency
    if (lat->pwm_timestamp >= lat->adc_timestamp) {
        lat->latency_cycles = lat->pwm_timestamp - lat->adc_timestamp;
    } else {
        lat->latency_cycles = (UINT32_MAX - lat->adc_timestamp) + lat->pwm_timestamp + 1;
    }
    
    // Update min/max
    if (lat->latency_cycles < lat->min_latency) {
        lat->min_latency = lat->latency_cycles;
    }
    if (lat->latency_cycles > lat->max_latency) {
        lat->max_latency = lat->latency_cycles;
    }
    
    // Update average
    if (lat->sample_count == 0) {
        lat->avg_latency = lat->latency_cycles;
    } else {
        lat->avg_latency = (lat->avg_latency * 15 + lat->latency_cycles) / 16;
    }
    
    lat->sample_count++;
    
    // Check latency violation
    if (PROFILER_CYCLES_TO_US(lat->latency_cycles) > MAX_ADC_TO_PWM_LATENCY_US) {
        lat->violations++;
    }
    
    // Clear ADC timestamp for next cycle
    lat->adc_timestamp = 0;
}

void Profiler_UpdateCPULoad(PerformanceProfiler_t* profiler, bool is_idle)
{
    if (profiler == NULL) return;
    
    // Simple CPU load estimation based on idle time
    // More accurate methods would use RTOS idle hook
    uint32_t now = HAL_GetTick();
    uint32_t delta_ms = now - profiler->last_update_ms;
    
    if (delta_ms >= 100) {  // Update every 100ms
        uint32_t total_cycles = profiler->total_cycles_busy + profiler->total_cycles_idle;
        if (total_cycles > 0) {
            profiler->cpu_load_percent = (uint32_t)((profiler->total_cycles_busy * 100) / total_cycles);
        }
        profiler->total_cycles_busy = 0;
        profiler->total_cycles_idle = 0;
        profiler->last_update_ms = now;
    }
    
    // Accumulate cycles (simplified - in practice would use cycle counter)
    if (is_idle) {
        profiler->total_cycles_idle += PROFILER_US_TO_CYCLES(100);  // Estimate
    } else {
        profiler->total_cycles_busy += PROFILER_US_TO_CYCLES(100);
    }
}

uint32_t Profiler_GetReport(PerformanceProfiler_t* profiler, char* buffer, uint32_t buffer_size)
{
    if (profiler == NULL || buffer == NULL || buffer_size == 0) return 0;
    
    int written = 0;
    int total = 0;
    
    // Header
    written = snprintf(buffer + total, buffer_size - total,
        "=== Performance Report ===\r\n"
        "CPU Load: %lu%%\r\n\r\n",
        (unsigned long)profiler->cpu_load_percent
    );
    if (written < 0) return 0;
    total += written;
    
    // Task statistics
    const char* task_names[] = {"Measure", "Control", "Safety"};
    for (int i = 0; i < TASK_ID_COUNT; i++) {
        TaskTimingStats_t* stats = &profiler->task_stats[i];
        uint32_t jitter_us = PROFILER_CYCLES_TO_US(Profiler_CalculateJitter((TaskProfilerID_t)i));
        
        written = snprintf(buffer + total, buffer_size - total,
            "Task: %s\r\n"
            "  Min: %lu µs\r\n"
            "  Max: %lu µs\r\n"
            "  Avg: %lu µs\r\n"
            "  Jitter: %lu µs\r\n"
            "  Violations: %lu\r\n"
            "  Samples: %lu\r\n\r\n",
            task_names[i],
            (unsigned long)PROFILER_CYCLES_TO_US(stats->min_cycles),
            (unsigned long)PROFILER_CYCLES_TO_US(stats->max_cycles),
            (unsigned long)PROFILER_CYCLES_TO_US(stats->avg_cycles),
            (unsigned long)jitter_us,
            (unsigned long)stats->violations,
            (unsigned long)stats->sample_count
        );
        if (written < 0) return 0;
        total += written;
    }
    
    // Latency statistics
    ADCToPWMLatency_t* lat = &profiler->latency_tracker;
    written = snprintf(buffer + total, buffer_size - total,
        "ADC-to-PWM Latency:\r\n"
        "  Min: %lu µs\r\n"
        "  Max: %lu µs\r\n"
        "  Avg: %lu µs\r\n"
        "  Violations: %lu\r\n"
        "  Samples: %lu\r\n\r\n",
        (unsigned long)PROFILER_CYCLES_TO_US(lat->min_latency),
        (unsigned long)PROFILER_CYCLES_TO_US(lat->max_latency),
        (unsigned long)PROFILER_CYCLES_TO_US(lat->avg_latency),
        (unsigned long)lat->violations,
        (unsigned long)lat->sample_count
    );
    if (written < 0) return 0;
    total += written;
    
    // Memory usage
    uint32_t heap_usage = Profiler_GetHeapUsagePercent();
    written = snprintf(buffer + total, buffer_size - total,
        "Memory Usage:\r\n"
        "  Heap: %lu%%\r\n",
        (unsigned long)heap_usage
    );
    if (written < 0) return 0;
    total += written;
    
    return (uint32_t)total;
}

void Profiler_Reset(PerformanceProfiler_t* profiler)
{
    if (profiler == NULL) return;
    
    memset(profiler->task_stats, 0, sizeof(profiler->task_stats));
    memset(&profiler->latency_tracker, 0, sizeof(ADCToPWMLatency_t));
    memset(g_cycle_buffers, 0, sizeof(g_cycle_buffers));
    
    // Re-initialize with defaults
    for (int i = 0; i < MAX_TASK_PROFILERS; i++) {
        profiler->task_stats[i].min_cycles = UINT32_MAX;
        profiler->task_stats[i].expected_cycles = EXPECTED_CYCLES[i];
    }
    profiler->latency_tracker.min_latency = UINT32_MAX;
}
