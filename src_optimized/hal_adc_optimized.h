/**
 * @file hal_adc_optimized.h
 * @brief Optimized ADC HAL with DMA Double-Buffering
 * 
 * Optimizations:
 * - DMA double-buffering for zero-copy operation
 * - Reduced interrupt latency
 * - Direct memory access without CPU overhead
 * - Optimized sample processing
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#ifndef HAL_ADC_OPTIMIZED_H
#define HAL_ADC_OPTIMIZED_H

#include "config_optimized.h"
#include "performance_profiler.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"

// =============================================================================
// ADC CONFIGURATION
// =============================================================================

#define ADC_CHANNEL_VIN         ADC_CHANNEL_0
#define ADC_CHANNEL_VOUT        ADC_CHANNEL_1
#define ADC_CHANNEL_CURRENT     ADC_CHANNEL_2
#define ADC_CHANNEL_TEMP        ADC_CHANNEL_3

// DMA buffer configuration for double-buffering
#define ADC_DMA_HALF_BUFFER     (ADC_DMA_BUFFER_SIZE / 2)

// =============================================================================
// DATA STRUCTURES
// =============================================================================

typedef struct {
    float vin;
    float vout;
    float current;
    float temperature;
    uint32_t timestamp;
    bool valid;
} ADC_Measurement_t;

typedef struct {
    ADC_HandleTypeDef hadc;
    DMA_HandleTypeDef hdma;
    
    // Double DMA buffer
    uint16_t dma_buffer[ADC_DMA_BUFFER_SIZE] __attribute__((aligned(4)));
    
    // Current processing buffer pointer
    volatile uint16_t* process_buffer;
    
    // Measurement data
    ADC_Measurement_t current;
    ADC_Measurement_t averaged;
    
    // State flags
    volatile bool conversion_complete;
    volatile bool half_complete;
    uint32_t sample_count;
    
    // Calibration coefficients
    float vin_gain, vin_offset;
    float vout_gain, vout_offset;
    float current_gain, current_offset;
} Adaptive_ADC_Opt_t;

// =============================================================================
// API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize optimized ADC with DMA double-buffering
 * @param adc Pointer to ADC handle
 * @return true on success, false on failure
 */
bool ADC_Opt_Init(Adaptive_ADC_Opt_t* adc);

/**
 * @brief Start ADC with DMA double-buffering
 * @param adc Pointer to initialized ADC
 * @return true on success, false on failure
 */
bool ADC_Opt_Start(Adaptive_ADC_Opt_t* adc);

/**
 * @brief Stop ADC and DMA
 * @param adc Pointer to running ADC
 * @return true on success, false on failure
 */
bool ADC_Opt_Stop(Adaptive_ADC_Opt_t* adc);

/**
 * @brief Process a DMA buffer half (call from ISR or task)
 * 
 * Optimized for low latency:
 * - Minimal branching
 * - Predictable memory access
 * - No floating-point in critical path
 * 
 * @param adc Pointer to ADC handle
 * @param is_first_half true = process first half, false = process second half
 */
void FAST_FUNC ADC_Opt_ProcessBuffer(Adaptive_ADC_Opt_t* adc, bool is_first_half);

/**
 * @brief Get latest measurement
 * @param adc Pointer to ADC handle
 * @param meas Pointer to measurement structure
 * @return true if valid data available
 */
bool ADC_Opt_GetMeasurement(Adaptive_ADC_Opt_t* adc, ADC_Measurement_t* meas);

/**
 * @brief Get averaged measurement
 * @param adc Pointer to ADC handle
 * @param meas Pointer to measurement structure
 * @return true if valid data available
 */
bool ADC_Opt_GetAveraged(Adaptive_ADC_Opt_t* adc, ADC_Measurement_t* meas);

/**
 * @brief Check if new data is ready
 * @param adc Pointer to ADC handle
 * @return true if new data available
 */
bool ADC_Opt_IsReady(Adaptive_ADC_Opt_t* adc);

/**
 * @brief Calibrate ADC using known references
 * @param adc Pointer to ADC handle
 * @param known_vin Known input voltage
 * @param known_vout Known output voltage
 * @param known_current Known current
 * @return true if calibration successful
 */
bool ADC_Opt_Calibrate(Adaptive_ADC_Opt_t* adc, float known_vin, float known_vout, float known_current);

/**
 * @brief Get raw ADC value for a channel
 * @param adc Pointer to ADC handle
 * @param channel Channel index (0-3)
 * @return Raw ADC value (0-4095)
 */
uint16_t ADC_Opt_GetRaw(const Adaptive_ADC_Opt_t* adc, uint8_t channel);

/**
 * @brief DMA half-complete callback (call from ISR)
 * @param adc Pointer to ADC handle
 */
void ADC_Opt_DMACallbackHalf(Adaptive_ADC_Opt_t* adc);

/**
 * @brief DMA complete callback (call from ISR)
 * @param adc Pointer to ADC handle
 */
void ADC_Opt_DMACallbackComplete(Adaptive_ADC_Opt_t* adc);

// =============================================================================
// INLINE CONVERSION FUNCTIONS
// =============================================================================

/**
 * @brief Fast ADC to voltage conversion
 */
static FORCE_INLINE float ADC_Opt_RawToVoltage(uint16_t raw) {
    // Pre-computed constant: 3300.0 / 4096.0 / 1000.0 = 0.00080566
    return raw * 0.000805664f;
}

/**
 * @brief Fast ADC to current conversion (for current sense)
 */
static FORCE_INLINE float ADC_Opt_RawToCurrent(uint16_t raw) {
    // voltage = raw * 3.3 / 4096
    // current = voltage / 0.01 (10mΩ shunt)
    return raw * 0.0805664f;  // Pre-computed: 0.000805664 / 0.01
}

/**
 * @brief Fast ADC to temperature conversion (simplified)
 */
static FORCE_INLINE float ADC_Opt_RawToTemp(uint16_t raw) {
    // Simplified linear conversion
    // 25°C = 760mV, 2.5mV per degree
    float voltage_mv = raw * (3300.0f / 4096.0f);
    return (voltage_mv - 760.0f) / 2.5f + 25.0f;
}

#endif // HAL_ADC_OPTIMIZED_H
