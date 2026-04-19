/**
 * @file hal_adc_optimized.c
 * @brief Optimized ADC Implementation with DMA Double-Buffering
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#include "hal_adc_optimized.h"

// Static moving average buffer (reduced size for lower latency)
#if ADC_FILTER_MOVING_AVG_ENABLED
static float mv_avg_buffer[ADC_NUM_CHANNELS][ADC_FILTER_MOVING_AVG_SIZE];
static uint8_t mv_avg_index = 0;
#endif

// IIR filter state
static float iir_state[ADC_NUM_CHANNELS] = {0};

bool ADC_Opt_Init(Adaptive_ADC_Opt_t* adc)
{
    if (adc == NULL) return false;
    
    memset(adc, 0, sizeof(Adaptive_ADC_Opt_t));
    
    // Initialize calibration to unity
    adc->vin_gain = 1.0f; adc->vin_offset = 0.0f;
    adc->vout_gain = 1.0f; adc->vout_offset = 0.0f;
    adc->current_gain = 1.0f; adc->current_offset = 0.0f;
    
    // GPIO initialization
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // ADC clock enable
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    // ADC configuration for maximum performance
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
    
    // Vin - fast sampling (low impedance source)
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_FAST;
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // Vout - fast sampling
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 2;
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // Current - medium sampling
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = 3;
    sConfig.SamplingTime = ADC_SAMPLETIME_MEDIUM;
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // Temperature - slow sampling (NTC thermal mass)
    sConfig.Channel = ADC_CHANNEL_3;
    sConfig.Rank = 4;
    sConfig.SamplingTime = ADC_SAMPLETIME_SLOW;
    HAL_ADC_ConfigChannel(&adc->hadc, &sConfig);
    
    // DMA configuration for double-buffering (circular mode)
    adc->hdma.Instance = DMA2_Stream0;
    adc->hdma.Init.Channel = DMA_CHANNEL_0;
    adc->hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    adc->hdma.Init.PeriphInc = DMA_PINC_DISABLE;
    adc->hdma.Init.MemInc = DMA_MINC_ENABLE;
    adc->hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    adc->hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    adc->hdma.Init.Mode = DMA_CIRCULAR;  // Double-buffering effect
    adc->hdma.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    adc->hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;  // Direct mode for lower latency
    
    HAL_DMA_Init(&adc->hdma);
    __HAL_LINKDMA(&adc->hadc, DMA_Handle, adc->hdma);
    
    // Initialize moving average buffer
#if ADC_FILTER_MOVING_AVG_ENABLED
    memset(mv_avg_buffer, 0, sizeof(mv_avg_buffer));
#endif
    memset(iir_state, 0, sizeof(iir_state));
    
    return true;
}

bool ADC_Opt_Start(Adaptive_ADC_Opt_t* adc)
{
    if (adc == NULL) return false;
    
    // Clear DMA buffer
    memset(adc->dma_buffer, 0, sizeof(adc->dma_buffer));
    
    // Start DMA in circular mode (enables double-buffering effect)
    HAL_StatusTypeDef status = HAL_ADC_Start_DMA(&adc->hadc, 
                                                 (uint32_t*)adc->dma_buffer, 
                                                 ADC_DMA_BUFFER_SIZE);
    
    return (status == HAL_OK);
}

bool ADC_Opt_Stop(Adaptive_ADC_Opt_t* adc)
{
    if (adc == NULL) return false;
    
    HAL_ADC_Stop_DMA(&adc->hadc);
    return true;
}

#if ADC_FILTER_MOVING_AVG_ENABLED
static float CalculateMovingAvg(float new_sample, uint8_t channel)
{
    mv_avg_buffer[channel][mv_avg_index] = new_sample;
    
    float sum = 0.0f;
    // Unroll loop for better performance
    sum += mv_avg_buffer[channel][0];
    sum += mv_avg_buffer[channel][1];
    sum += mv_avg_buffer[channel][2];
    sum += mv_avg_buffer[channel][3];
    
    return sum * 0.25f;  // Divide by 4 (faster than / ADC_FILTER_MOVING_AVG_SIZE)
}
#endif

void FAST_FUNC ADC_Opt_ProcessBuffer(Adaptive_ADC_Opt_t* adc, bool is_first_half)
{
    if (adc == NULL) return;
    
    // Mark ADC complete for profiling
    PROFILE_ADC_COMPLETE();
    
    // Select buffer to process
    uint16_t* buffer = is_first_half ? adc->dma_buffer : &adc->dma_buffer[ADC_DMA_HALF_BUFFER];
    uint16_t samples = ADC_DMA_HALF_BUFFER / ADC_NUM_CHANNELS;
    
    // Fast sum calculation with loop unrolling
    uint32_t sum_vin = 0, sum_vout = 0, sum_current = 0, sum_temp = 0;
    
    // Process samples with loop unrolling (4 samples at a time)
    for (uint16_t i = 0; i < ADC_DMA_HALF_BUFFER; i += 16) {  // 4 channels * 4 samples
        // Sample 0
        sum_vin += buffer[i + 0];
        sum_vout += buffer[i + 1];
        sum_current += buffer[i + 2];
        sum_temp += buffer[i + 3];
        // Sample 1
        sum_vin += buffer[i + 4];
        sum_vout += buffer[i + 5];
        sum_current += buffer[i + 6];
        sum_temp += buffer[i + 7];
        // Sample 2
        sum_vin += buffer[i + 8];
        sum_vout += buffer[i + 9];
        sum_current += buffer[i + 10];
        sum_temp += buffer[i + 11];
        // Sample 3
        sum_vin += buffer[i + 12];
        sum_vout += buffer[i + 13];
        sum_current += buffer[i + 14];
        sum_temp += buffer[i + 15];
    }
    
    // Calculate averages (use fixed-point for intermediate)
    float avg_vin_raw = (float)sum_vin / (float)samples;
    float avg_vout_raw = (float)sum_vout / (float)samples;
    float avg_current_raw = (float)sum_current / (float)samples;
    float avg_temp_raw = (float)sum_temp / (float)samples;
    
    // Convert to physical units (fast inline conversions)
    float vin_volts = ADC_Opt_RawToVoltage((uint16_t)avg_vin_raw) * adc->vin_gain + adc->vin_offset;
    float vout_volts = ADC_Opt_RawToVoltage((uint16_t)avg_vout_raw) * adc->vout_gain + adc->vout_offset;
    float current_amps = ADC_Opt_RawToCurrent((uint16_t)avg_current_raw) * adc->current_gain + adc->current_offset;
    float temp_c = ADC_Opt_RawToTemp((uint16_t)avg_temp_raw);
    
    // Apply filtering with optimized IIR
#if ADC_FILTER_IIR_ENABLED
    // Faster IIR: alpha = 0.15 → multiply by 3/20 approximated
    float alpha = ADC_FILTER_IIR_ALPHA;
    
    iir_state[0] += alpha * (vin_volts - iir_state[0]);
    iir_state[1] += alpha * (vout_volts - iir_state[1]);
    iir_state[2] += alpha * (current_amps - iir_state[2]);
    iir_state[3] += alpha * (temp_c - iir_state[3]);
    
    adc->current.vin = iir_state[0];
    adc->current.vout = iir_state[1];
    adc->current.current = iir_state[2];
    adc->current.temperature = iir_state[3];
#else
    adc->current.vin = vin_volts;
    adc->current.vout = vout_volts;
    adc->current.current = current_amps;
    adc->current.temperature = temp_c;
#endif
    
    adc->current.timestamp = HAL_GetTick();
    adc->current.valid = true;
    
    // Update averaged values (slower IIR)
    float avg_alpha = 0.05f;
    adc->averaged.vin += avg_alpha * (adc->current.vin - adc->averaged.vin);
    adc->averaged.vout += avg_alpha * (adc->current.vout - adc->averaged.vout);
    adc->averaged.current += avg_alpha * (adc->current.current - adc->averaged.current);
    adc->averaged.temperature += avg_alpha * (adc->current.temperature - adc->averaged.temperature);
    adc->averaged.timestamp = adc->current.timestamp;
    adc->averaged.valid = true;
    
    adc->sample_count += samples;
    adc->conversion_complete = true;
    adc->half_complete = is_first_half;
}

bool ADC_Opt_GetMeasurement(Adaptive_ADC_Opt_t* adc, ADC_Measurement_t* meas)
{
    if (adc == NULL || meas == NULL || !adc->current.valid) {
        return false;
    }
    
    memcpy(meas, &adc->current, sizeof(ADC_Measurement_t));
    adc->conversion_complete = false;  // Clear flag after reading
    return true;
}

bool ADC_Opt_GetAveraged(Adaptive_ADC_Opt_t* adc, ADC_Measurement_t* meas)
{
    if (adc == NULL || meas == NULL || !adc->averaged.valid) {
        return false;
    }
    
    memcpy(meas, &adc->averaged, sizeof(ADC_Measurement_t));
    return true;
}

bool ADC_Opt_IsReady(Adaptive_ADC_Opt_t* adc)
{
    if (adc == NULL) return false;
    return adc->conversion_complete;
}

bool ADC_Opt_Calibrate(Adaptive_ADC_Opt_t* adc, float known_vin, float known_vout, float known_current)
{
    if (adc == NULL || !adc->current.valid) return false;
    
    // Simple linear calibration (offset assumed zero, gain calculated)
    if (adc->current.vin > 0.1f) {
        adc->vin_gain = known_vin / adc->current.vin;
    }
    if (adc->current.vout > 0.1f) {
        adc->vout_gain = known_vout / adc->current.vout;
    }
    if (adc->current.current > 0.01f) {
        adc->current_gain = known_current / adc->current.current;
    }
    
    return true;
}

uint16_t ADC_Opt_GetRaw(const Adaptive_ADC_Opt_t* adc, uint8_t channel)
{
    if (adc == NULL || channel >= ADC_NUM_CHANNELS) return 0;
    return adc->dma_buffer[channel];  // Most recent sample
}

void ADC_Opt_DMACallbackHalf(Adaptive_ADC_Opt_t* adc)
{
    if (adc == NULL) return;
    // First half complete - can be processed in ISR or signaled to task
    adc->half_complete = true;
}

void ADC_Opt_DMACallbackComplete(Adaptive_ADC_Opt_t* adc)
{
    if (adc == NULL) return;
    // Second half complete
    adc->half_complete = true;
}
