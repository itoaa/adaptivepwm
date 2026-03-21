/**
 * @file hal_adc.h
 * @brief Hardware Abstraction Layer for ADC with DMA
 * 
 * Implements continuous ADC sampling using DMA for zero-CPU overhead.
 * Supports multiple channels: voltage, current, temperature.
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// ADC Channels (STM32F401RE ADC1)
#define ADC_CHANNEL_VIN         ADC_CHANNEL_0   // PA0 - Input voltage
#define ADC_CHANNEL_VOUT        ADC_CHANNEL_1   // PA1 - Output voltage  
#define ADC_CHANNEL_CURRENT     ADC_CHANNEL_2   // PA2 - Current sense
#define ADC_CHANNEL_TEMP        ADC_CHANNEL_3   // PA3 - Temperature

#define ADC_NUM_CHANNELS        4
#define ADC_DMA_BUFFER_SIZE     64   // Must be multiple of num channels

// Voltage reference (STM32F401 internal)
#define ADC_VREF_MV             3300.0f
#define ADC_RESOLUTION          4096.0f  // 12-bit

// Current sense resistor
#define CURRENT_SENSE_OHMS      0.01f    // 10mΩ shunt
#define CURRENT_AMP_PER_V       (1.0f / CURRENT_SENSE_OHMS)

// Temperature coefficients
#define TEMP_COEFF_MV_PER_C     2.5f
#define TEMP_OFFSET_MV          760.0f   // 25°C = 760mV

typedef struct {
    float vin;          // Input voltage (V)
    float vout;         // Output voltage (V)
    float current;      // Current (A)
    float temperature;  // Temperature (°C)
    uint32_t timestamp; // Timestamp in ms
    bool valid;         // Data valid flag
} ADC_Measurement_t;

typedef struct {
    ADC_HandleTypeDef hadc;
    DMA_HandleTypeDef hdma;
    uint16_t dma_buffer[ADC_DMA_BUFFER_SIZE];
    ADC_Measurement_t current;
    ADC_Measurement_t averaged;
    volatile bool conversion_complete;
    uint32_t sample_count;
} Adaptive_ADC_t;

// NOTE: Using Adaptive_ prefix to avoid conflicts with STM32 HAL functions

bool Adaptive_ADC_Init(Adaptive_ADC_t* adc);
bool Adaptive_ADC_Start_DMA(Adaptive_ADC_t* adc);
bool Adaptive_ADC_Stop_DMA(Adaptive_ADC_t* adc);
bool Adaptive_ADC_GetMeasurement(Adaptive_ADC_t* adc, ADC_Measurement_t* meas);
bool Adaptive_ADC_GetAveraged(Adaptive_ADC_t* adc, ADC_Measurement_t* meas);
void Adaptive_ADC_ProcessBuffer(Adaptive_ADC_t* adc);
bool Adaptive_ADC_IsReady(Adaptive_ADC_t* adc);
bool Adaptive_ADC_Calibrate(Adaptive_ADC_t* adc, float known_vin, float known_vout, float known_current);
uint16_t Adaptive_ADC_GetRaw(const Adaptive_ADC_t* adc, uint8_t channel);

#endif // HAL_ADC_H
