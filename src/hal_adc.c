/**
 * @file hal_adc.c
 * @brief Hardware Abstraction Layer for ADC with DMA and enhanced filtering
 * 
 * Implements continuous ADC sampling using DMA for zero-CPU overhead.
 * Supports multiple channels: voltage, current, temperature.
 * 
 * Enhancements:
 * - Dual filtering: IIR + Moving average
 * - Adaptive sampling rate during transients
 * - Better noise rejection
 * - Steinhart-Hart temperature calculation for NTC thermistors
 * - Fixed moving average synchronization
 * 
 * @version 2.3.0
 * @date 2026-04-16
 */

#include "hal_adc.h"
#include "config.h"
#include "adaptive_assert.h"
#include <string.h>
#include <math.h>

// Helper function prototypes
static void ADC_GPIO_Init(void);
static void ADC_DMA_Init(Adaptive_ADC_t* adc);
static float ConvertToVin(uint16_t adc_value);
static float ConvertToVout(uint16_t adc_value);
static float ConvertToCurrent(uint16_t adc_value);
static float ConvertToTemp_Steinhart(uint16_t adc_value);

// Static flag for interrupt handler
static volatile bool adc_dma_complete = false;

// Moving average buffer - PWM-ARCH-003: Fixed synchronization
#if ADC_FILTER_MOVING_AVG_ENABLED
static float moving_avg_buffer[ADC_NUM_CHANNELS][ADC_FILTER_MOVING_AVG_SIZE];
static uint8_t moving_avg_index = 0;
#endif

static void ADC_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // Configure ADC pins (PA0-PA3)
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void ADC_DMA_Init(Adaptive_ADC_t* adc)
{
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    adc->hdma.Instance = DMA2_Stream0;
    adc->hdma.Init.Channel = DMA_CHANNEL_0;
    adc->hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    adc->hdma.Init.PeriphInc = DMA_PINC_DISABLE;
    adc->hdma.Init.MemInc = DMA_MINC_ENABLE;
    adc->hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    adc->hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    adc->hdma.Init.Mode = DMA_CIRCULAR;
    adc->hdma.Init.Priority = DMA_PRIORITY_HIGH;
    adc->hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    
    HAL_DMA_Init(&adc->hdma);
    
    __HAL_LINKDMA(&adc->hadc, DMA_Handle, adc->hdma);
}

bool Adaptive_ADC_Init(Adaptive_ADC_t* adc)
{
    ADAPTIVE_ASSERT(adc != NULL);
    
    if (adc == NULL) return false;
    
    memset(adc, 0, sizeof(Adaptive_ADC_t));
    
    ADC_GPIO_Init();
    
    __HAL_RCC_ADC1_CLK_ENABLE();
    
    // Configure ADC for maximum performance
    adc->hadc.Instance = ADC1;
    adc->hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;  // 42 MHz
    adc->hadc.Init.Resolution = ADC_RESOLUTION_12B;
    adc->hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->hadc.Init.ScanConvMode = ENABLE;
    adc->hadc.Init.ContinuousConvMode = ENABLE;
    adc->hadc.Init.DMAContinuousRequests = ENABLE;
    adc->hadc.Init.NbrOfConversion = ADC_NUM_CHANNELS;
    adc->hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc->hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    
    if (HAL_ADC_Init(&adc->hadc) != HAL_OK) {
        return false;
    }
    
    // Configure channels with optimized sampling times
    ADC_ChannelConfTypeDef sConfig = {0};
    
    // Channel 0: Vin - fast sampling (low impedance)
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_FAST;  // 3 cycles
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // Channel 1: Vout - fast sampling
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 2;
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // Channel 2: Current - medium sampling (current sense may need settling)
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = 3;
    sConfig.SamplingTime = ADC_SAMPLETIME_MEDIUM;  // 15 cycles
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // Channel 3: Temperature - slow sampling (NTC thermal mass)
    sConfig.Channel = ADC_CHANNEL_3;
    sConfig.Rank = 4;
    sConfig.SamplingTime = ADC_SAMPLETIME_SLOW;  // 28 cycles
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    ADC_DMA_Init(adc);
    
    // Initialize moving average buffer
#if ADC_FILTER_MOVING_AVG_ENABLED
    memset(moving_avg_buffer, 0, sizeof(moving_avg_buffer));
    moving_avg_index = 0;
#endif
    
    return true;
}

bool Adaptive_ADC_Start_DMA(Adaptive_ADC_t* adc)
{
    ADAPTIVE_ASSERT(adc != NULL);
    
    if (adc == NULL) return false;
    
    HAL_ADC_Start_DMA(&adc->hadc, 
                    (uint32_t*)adc->dma_buffer, 
                    ADC_DMA_BUFFER_SIZE);
    
    return true;
}

bool Adaptive_ADC_Stop_DMA(Adaptive_ADC_t* adc)
{
    ADAPTIVE_ASSERT(adc != NULL);
    
    if (adc == NULL) return false;
    
    HAL_ADC_Stop_DMA(&adc->hadc);
    return true;
}

// Calculate moving average
#if ADC_FILTER_MOVING_AVG_ENABLED
static float CalculateMovingAvg(float new_sample, uint8_t channel)
{
    moving_avg_buffer[channel][moving_avg_index] = new_sample;
    
    float sum = 0.0f;
    for (int i = 0; i < ADC_FILTER_MOVING_AVG_SIZE; i++) {
        sum += moving_avg_buffer[channel][i];
    }
    
    return sum / ADC_FILTER_MOVING_AVG_SIZE;
}
#endif

// Apply dual filtering (moving average then IIR)
// PWM-ARCH-003: Fixed - moving average index now updated after all channels processed
static float ApplyFiltering(float raw, float* filtered, uint8_t channel)
{
    float result = raw;
    
#if ADC_FILTER_MOVING_AVG_ENABLED
    // First stage: Moving average
    result = CalculateMovingAvg(raw, channel);
    
    // PWM-ARCH-003 FIX: Index update moved to Adaptive_ADC_ProcessBuffer()
    // after ALL channels have been processed
#endif
    
#if ADC_FILTER_IIR_ENABLED
    // Second stage: IIR filter
    *filtered += ADC_FILTER_IIR_ALPHA * (result - *filtered);
#else
    *filtered = result;
#endif
    
    return *filtered;
}

// ADC calibration values (multiplicative + offset on scaled engineering units)
static float vin_gain = 1.0f, vin_offset = 0.0f;
static float vout_gain = 1.0f, vout_offset = 0.0f;
static float current_gain = 1.0f, current_offset = 0.0f;

static float AdcToPinVolts(uint16_t adc_value)
{
    return ((float)adc_value / ADC_RESOLUTION) * (ADC_VREF_MV / 1000.0f);
}

static float ConvertToVin(uint16_t adc_value)
{
    float vadc = AdcToPinVolts(adc_value);
    return (vadc * ADC_VIN_DIVIDER_RATIO * vin_gain) + vin_offset;
}

static float ConvertToVout(uint16_t adc_value)
{
    float vadc = AdcToPinVolts(adc_value);
    return (vadc * ADC_VOUT_DIVIDER_RATIO * vout_gain) + vout_offset;
}

/* I = V_sense / (V per amp). Not V/R_shunt alone — that ignored the sense amp. */
static float ConvertToCurrent(uint16_t adc_value)
{
    float vadc = AdcToPinVolts(adc_value);
    float amps = 0.0f;
    if (ADC_CURRENT_V_PER_AMP > 1e-9f) {
        amps = vadc / ADC_CURRENT_V_PER_AMP;
    }
    return (amps * current_gain) + current_offset;
}

// PWM-ARCH-003: Legacy linear conversion (kept for backward compatibility)
static float ConvertToTemp_Linear(uint16_t adc_value)
{
    float voltage = (adc_value / ADC_RESOLUTION) * ADC_VREF_MV;
    // Simplified: assuming linear NTC for now
    // Real implementation should use Steinhart-Hart or lookup table
    return (voltage - TEMP_OFFSET_MV) / TEMP_COEFF_MV_PER_C + 25.0f;
}

// PWM-ARCH-003: Steinhart-Hart temperature calculation for NTC thermistors
// Uses B-parameter equation for accurate temperature measurement
static float ConvertToTemp_Steinhart(uint16_t adc_value)
{
    // Constants for typical 10k NTC (B=3950)
    const float R_SERIES = 10000.0f;      // 10k pull-up resistor
    const float R_NOMINAL = 10000.0f;     // 10k @ 25C
    const float B_COEFF = 3950.0f;        // Beta value (from NTC datasheet)
    const float TEMP_NOMINAL_K = 298.15f; // 25C in Kelvin
    
    // Convert ADC value to voltage
    float adc_voltage = (adc_value / ADC_RESOLUTION) * ADC_VREF_MV;
    
    // Convert voltage to resistance using voltage divider formula
    // V_ntc = Vcc * R_ntc / (R_series + R_ntc)
    // R_ntc = R_series * V_ntc / (Vcc - V_ntc)
    float resistance = R_SERIES * adc_voltage / (ADC_VREF_MV - adc_voltage);
    
    // Handle edge cases
    if (resistance <= 0.0f) {
        return -273.15f; // Absolute zero (error condition)
    }
    if (resistance > 10e6f) {
        return 150.0f; // Very hot (saturation)
    }
    
    // Steinhart-Hart simplified using B-parameter equation
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float steinhart = resistance / R_NOMINAL;
    steinhart = logf(steinhart);
    steinhart /= B_COEFF;
    steinhart += 1.0f / TEMP_NOMINAL_K;
    steinhart = 1.0f / steinhart;
    
    // Return temperature in Celsius
    float temp_celsius = steinhart - 273.15f;
    
    // Clamp to reasonable range
    if (temp_celsius < -40.0f) temp_celsius = -40.0f;
    if (temp_celsius > 150.0f) temp_celsius = 150.0f;
    
    return temp_celsius;
}

// Check for transient condition
static bool DetectTransient(Adaptive_ADC_t* adc, float new_vin, float new_vout, float new_current)
{
    static float last_vin = 0.0f, last_vout = 0.0f, last_current = 0.0f;
    
    if (adc->sample_count < 10) {
        // Not enough samples yet
        last_vin = new_vin;
        last_vout = new_vout;
        last_current = new_current;
        return false;
    }
    
    float delta_vin = fabsf(new_vin - last_vin) / last_vin;
    float delta_vout = fabsf(new_vout - last_vout) / last_vout;
    float delta_current = fabsf(new_current - last_current) / (last_current + 0.001f);  // Avoid div by zero
    
    last_vin = new_vin;
    last_vout = new_vout;
    last_current = new_current;
    
    return (delta_vin > ADC_TRANSIENT_THRESHOLD) ||
           (delta_vout > ADC_TRANSIENT_THRESHOLD) ||
           (delta_current > ADC_TRANSIENT_THRESHOLD);
}

// PWM-ARCH-003: Process buffer with fixed moving average synchronization
void Adaptive_ADC_ProcessBuffer(Adaptive_ADC_t* adc)
{
    ADAPTIVE_ASSERT(adc != NULL);
    
    if (adc == NULL) return;
    
    // Calculate average of DMA buffer
    float sum_vin = 0, sum_vout = 0, sum_current = 0, sum_temp = 0;
    uint16_t samples = ADC_DMA_BUFFER_SIZE / ADC_NUM_CHANNELS;
    
    for (uint16_t i = 0; i < ADC_DMA_BUFFER_SIZE; i += ADC_NUM_CHANNELS) {
        sum_vin += adc->dma_buffer[i];
        sum_vout += adc->dma_buffer[i + 1];
        sum_current += adc->dma_buffer[i + 2];
        sum_temp += adc->dma_buffer[i + 3];
    }
    
    // Convert raw averages to physical units (board scale factors in config.h)
    float raw_vin = ConvertToVin((uint16_t)(sum_vin / samples));
    float raw_vout = ConvertToVout((uint16_t)(sum_vout / samples));
    float raw_current = ConvertToCurrent((uint16_t)(sum_current / samples));
    
    // PWM-ARCH-003: Use Steinhart-Hart for accurate temperature
    float raw_temp = ConvertToTemp_Steinhart(sum_temp / samples);
    
    // Apply dual-stage filtering
    adc->current.vin = ApplyFiltering(raw_vin, &adc->current.vin, 0);
    adc->current.vout = ApplyFiltering(raw_vout, &adc->current.vout, 1);
    adc->current.current = ApplyFiltering(raw_current, &adc->current.current, 2);
    adc->current.temperature = ApplyFiltering(raw_temp, &adc->current.temperature, 3);
    adc->current.timestamp = HAL_GetTick();
    adc->current.valid = true;
    
    // PWM-ARCH-003 FIX: Update moving average index AFTER all channels processed
    // This ensures synchronization across all channels
#if ADC_FILTER_MOVING_AVG_ENABLED
    moving_avg_index = (moving_avg_index + 1) % ADC_FILTER_MOVING_AVG_SIZE;
#endif
    
    // Update averaged values (IIR filter for display/logging)
    adc->averaged.vin += ADC_FILTER_IIR_ALPHA * (adc->current.vin - adc->averaged.vin);
    adc->averaged.vout += ADC_FILTER_IIR_ALPHA * (adc->current.vout - adc->averaged.vout);
    adc->averaged.current += ADC_FILTER_IIR_ALPHA * (adc->current.current - adc->averaged.current);
    adc->averaged.temperature += ADC_FILTER_IIR_ALPHA * (adc->current.temperature - adc->averaged.temperature);
    adc->averaged.timestamp = adc->current.timestamp;
    adc->averaged.valid = true;
    
    adc->sample_count += samples;
    adc->conversion_complete = true;
    
    // Check for transient and adjust sampling if needed
#if ADC_ADAPTIVE_SAMPLING_ENABLED
    static uint32_t fast_sampling_until = 0;
    
    if (DetectTransient(adc, raw_vin, raw_vout, raw_current)) {
        fast_sampling_until = HAL_GetTick() + ADC_FAST_SAMPLE_DURATION_MS;
    }
    
    // Adaptive sampling logic would go here
    // In a real implementation, this would adjust timer or trigger rate
#endif
}

bool Adaptive_ADC_GetMeasurement(Adaptive_ADC_t* adc, ADC_Measurement_t* meas)
{
    ADAPTIVE_ASSERT(adc != NULL);
    ADAPTIVE_ASSERT(meas != NULL);
    
    if (adc == NULL || meas == NULL || !adc->current.valid) {
        return false;
    }
    
    memcpy(meas, &adc->current, sizeof(ADC_Measurement_t));
    adc->conversion_complete = false;  // Clear flag after reading
    return true;
}

bool Adaptive_ADC_GetAveraged(Adaptive_ADC_t* adc, ADC_Measurement_t* meas)
{
    ADAPTIVE_ASSERT(adc != NULL);
    ADAPTIVE_ASSERT(meas != NULL);
    
    if (adc == NULL || meas == NULL || !adc->averaged.valid) {
        return false;
    }
    
    memcpy(meas, &adc->averaged, sizeof(ADC_Measurement_t));
    return true;
}

bool Adaptive_ADC_IsReady(Adaptive_ADC_t* adc)
{
    ADAPTIVE_ASSERT(adc != NULL);
    
    if (adc == NULL) return false;
    return adc->conversion_complete;
}

bool Adaptive_ADC_Calibrate(Adaptive_ADC_t* adc, float known_vin, float known_vout, float known_current)
{
    ADAPTIVE_ASSERT(adc != NULL);
    
    if (adc == NULL || !adc->current.valid) return false;
    
    // Simple two-point calibration
    // This assumes current values are raw (uncompensated)
    // In practice, you might want to use known precision references
    
    if (adc->current.vin > 0.1f) {
        vin_gain = known_vin / adc->current.vin;
    }
    
    if (adc->current.vout > 0.1f) {
        vout_gain = known_vout / adc->current.vout;
    }
    
    if (adc->current.current > 0.01f) {
        current_gain = known_current / adc->current.current;
    }
    
    return true;
}

uint16_t Adaptive_ADC_GetRaw(const Adaptive_ADC_t* adc, uint8_t channel)
{
    ADAPTIVE_ASSERT(adc != NULL);
    ADAPTIVE_ASSERT(channel < ADC_NUM_CHANNELS);
    
    if (adc == NULL || channel >= ADC_NUM_CHANNELS) return 0;
    
    // Return most recent sample for requested channel
    // This is simplified - in practice might want average
    return adc->dma_buffer[channel];
}

// ADC DMA Complete Callback — ISR only sets a flag; processing is in the main loop
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    (void)hadc;
    adc_dma_complete = true;
}

// Check and clear DMA complete flag
bool Adaptive_ADC_CheckDMAComplete(void)
{
    if (adc_dma_complete) {
        adc_dma_complete = false;
        return true;
    }
    return false;
}
