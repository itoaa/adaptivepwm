/**
 * @file hal_adc.c
 * @brief ADC with DMA implementation - Optimized Clock Configuration
 * 
 * ADC Clock: 42 MHz (max) from PCLK2/2
 * Sampling: Optimized for PWM-synchronized measurements
 * 
 * @version 2.1.0
 * @date 2026-03-21
 */

#include "hal_adc.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

// Conversion factors (will be calibrated)
static float vin_gain = ADC_VREF_MV / ADC_RESOLUTION / 1000.0f;  // V per count
static float vout_gain = ADC_VREF_MV / ADC_RESOLUTION / 1000.0f;
static float current_gain = ADC_VREF_MV / ADC_RESOLUTION * CURRENT_AMP_PER_V;
static float current_offset = 0.0f;  // For zero-current calibration

/**
 * @brief Initialize ADC GPIO pins
 */
static void ADC_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // PA0-PA3 as analog inputs
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
 * @brief Initialize DMA for ADC
 */
static bool ADC_DMA_Init(Adaptive_ADC_t* adc)
{
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    // DMA2 Stream 0 Channel 0 for ADC1
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
    
    if (HAL_DMA_Init(&adc->hdma) != HAL_OK) {
        return false;
    }
    
    __HAL_LINKDMA(&adc->hadc, DMA_Handle, adc->hdma);
    
    // Enable DMA interrupt
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    
    return true;
}

/**
 * @brief Initialize ADC with optimized clock settings
 * 
 * Clock Configuration:
 *   ADC Clock = PCLK2 / 2 = 84 MHz / 2 = 42 MHz (maximum)
 *   
 * Sampling Times (at 42 MHz):
 *   3 cycles  = 71 ns  (fastest)
 *   15 cycles = 357 ns
 *   28 cycles = 667 ns (default)
 *   
 * Total conversion time = Sampling + 12 cycles (ADC resolution)
 *   3 cycles:  15 cycles = 357 ns
 *   15 cycles: 27 cycles = 643 ns
 *   28 cycles: 40 cycles = 952 ns
 */
bool Adaptive_ADC_Init(Adaptive_ADC_t* adc)
{
    if (adc == NULL) {
        return false;
    }
    
    memset(adc, 0, sizeof(Adaptive_ADC_t));
    
    __HAL_RCC_ADC1_CLK_ENABLE();
    
    ADC_GPIO_Init();
    
    adc->hadc.Instance = ADC1;
    // Optimized: ADC clock = PCLK2/2 = 42 MHz (max allowed)
    adc->hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    adc->hadc.Init.Resolution = ADC_RESOLUTION_12B;
    adc->hadc.Init.ScanConvMode = ENABLE;
    adc->hadc.Init.ContinuousConvMode = ENABLE;
    adc->hadc.Init.DMAContinuousRequests = ENABLE;
    adc->hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->hadc.Init.NbrOfConversion = ADC_NUM_CHANNELS;
    adc->hadc.Init.DiscontinuousConvMode = DISABLE;
    adc->hadc.Init.NbrOfDiscConversion = 0;
    // No external trigger - free running for now
    adc->hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc->hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    
    if (HAL_ADC_Init(&adc->hadc) != HAL_OK) {
        return false;
    }
    
    // Configure channels with optimized sampling times
    ADC_ChannelConfTypeDef sConfig = {0};
    
    // Vin - PA0 (Channel 0)
    // Fast sampling for voltage (low impedance)
    sConfig.Channel = ADC_CHANNEL_VIN;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;  // 71 ns @ 42 MHz
    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) return false;
    
    // Vout - PA1 (Channel 1)
    sConfig.Channel = ADC_CHANNEL_VOUT;
    sConfig.Rank = 2;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;  // 71 ns @ 42 MHz
    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) return false;
    
    // Current - PA2 (Channel 2)
    // Slightly longer sampling for current sense (may have higher impedance)
    sConfig.Channel = ADC_CHANNEL_CURRENT;
    sConfig.Rank = 3;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;  // 357 ns @ 42 MHz
    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) return false;
    
    // Temperature - PA3 (Channel 3)
    // Temperature is slow-changing, use longer sampling
    sConfig.Channel = ADC_CHANNEL_TEMP;
    sConfig.Rank = 4;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;  // 667 ns @ 42 MHz
    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) return false;
    
    if (!ADC_DMA_Init(adc)) {
        return false;
    }
    
    return true;
}

/**
 * @brief Start ADC DMA conversions
 */
bool Adaptive_ADC_Start_DMA(Adaptive_ADC_t* adc)
{
    if (adc == NULL) {
        return false;
    }
    
    if (HAL_ADC_Start_DMA(&adc->hadc, 
                          (uint32_t*)adc->dma_buffer, 
                          ADC_DMA_BUFFER_SIZE) != HAL_OK) {
        return false;
    }
    
    return true;
}

/**
 * @brief Stop ADC DMA conversions
 */
bool Adaptive_ADC_Stop_DMA(Adaptive_ADC_t* adc)
{
    if (adc == NULL) {
        return false;
    }
    
    HAL_ADC_Stop_DMA(&adc->hadc);
    return true;
}

/**
 * @brief Convert raw ADC value to voltage
 */
static float ConvertToVoltage(uint16_t raw)
{
    return raw * vin_gain;
}

/**
 * @brief Convert raw ADC value to current
 */
static float ConvertToCurrent(uint16_t raw)
{
    // Center at 1.65V (half of 3.3V) for bidirectional current
    float voltage = raw * (ADC_VREF_MV / ADC_RESOLUTION / 1000.0f);
    return (voltage - 1.65f) * current_gain - current_offset;
}

/**
 * @brief Convert raw ADC value to temperature
 */
static float ConvertToTemp(uint16_t raw)
{
    float voltage_mv = raw * (ADC_VREF_MV / ADC_RESOLUTION);
    return 25.0f + (voltage_mv - TEMP_OFFSET_MV) / TEMP_COEFF_MV_PER_C;
}

/**
 * @brief Process DMA buffer with averaged measurements
 */
void Adaptive_ADC_ProcessBuffer(Adaptive_ADC_t* adc)
{
    if (adc == NULL) return;
    
    // Process half-complete or complete buffer
    uint32_t sum_vin = 0, sum_vout = 0, sum_current = 0, sum_temp = 0;
    uint32_t samples = ADC_DMA_BUFFER_SIZE / ADC_NUM_CHANNELS;
    
    for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE; i += ADC_NUM_CHANNELS) {
        sum_vin += adc->dma_buffer[i];
        sum_vout += adc->dma_buffer[i + 1];
        sum_current += adc->dma_buffer[i + 2];
        sum_temp += adc->dma_buffer[i + 3];
    }
    
    // Update current measurement
    adc->current.vin = ConvertToVoltage(sum_vin / samples);
    adc->current.vout = ConvertToVoltage(sum_vout / samples);
    adc->current.current = ConvertToCurrent(sum_current / samples);
    adc->current.temperature = ConvertToTemp(sum_temp / samples);
    adc->current.timestamp = HAL_GetTick();
    adc->current.valid = true;
    
    // Update averaged values (simple IIR filter)
    float alpha = MEASUREMENT_ALPHA;  // From config
    adc->averaged.vin += alpha * (adc->current.vin - adc->averaged.vin);
    adc->averaged.vout += alpha * (adc->current.vout - adc->averaged.vout);
    adc->averaged.current += alpha * (adc->current.current - adc->averaged.current);
    adc->averaged.temperature += alpha * (adc->current.temperature - adc->averaged.temperature);
    
    adc->sample_count += samples;
    adc->conversion_complete = true;
}

/**
 * @brief Get current ADC measurement
 */
bool Adaptive_ADC_GetMeasurement(Adaptive_ADC_t* adc, ADC_Measurement_t* meas)
{
    if (adc == NULL || meas == NULL || !adc->current.valid) {
        return false;
    }
    
    memcpy(meas, &adc->current, sizeof(ADC_Measurement_t));
    adc->conversion_complete = false;
    return true;
}

/**
 * @brief Get averaged ADC measurement
 */
bool Adaptive_ADC_GetAveraged(Adaptive_ADC_t* adc, ADC_Measurement_t* meas)
{
    if (adc == NULL || meas == NULL || adc->sample_count == 0) {
        return false;
    }
    
    memcpy(meas, &adc->averaged, sizeof(ADC_Measurement_t));
    return true;
}

/**
 * @brief Check if ADC conversion is complete
 */
bool Adaptive_ADC_IsReady(Adaptive_ADC_t* adc)
{
    return (adc != NULL) && adc->conversion_complete;
}

/**
 * @brief Calibrate ADC with known values
 */
bool Adaptive_ADC_Calibrate(Adaptive_ADC_t* adc, float known_vin, float known_vout, float known_current)
{
    if (adc == NULL || !adc->current.valid) return false;
    
    // Calculate calibration gains for voltage
    if (adc->current.vin > 0 && known_vin > 0) {
        vin_gain *= known_vin / adc->current.vin;
    }
    if (adc->current.vout > 0 && known_vout > 0) {
        vout_gain *= known_vout / adc->current.vout;
    }
    
    // Calibrate current offset using known_current
    if (known_current >= 0) {
        float expected_voltage = 1.65f + (known_current / current_gain);
        float actual_voltage = adc->current.current / current_gain + 1.65f;
        current_offset = (expected_voltage - actual_voltage) * current_gain;
    }
    
    return true;
}

/**
 * @brief Get raw ADC value for a channel
 */
uint16_t Adaptive_ADC_GetRaw(const Adaptive_ADC_t* adc, uint8_t channel)
{
    if (adc == NULL || channel >= ADC_NUM_CHANNELS) {
        return 0;
    }
    return adc->dma_buffer[channel];  // Latest value for channel
}
