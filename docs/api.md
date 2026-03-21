# API Documentation

## Overview

This document describes the Application Programming Interface for the AdaptivePWM system (v2.1.0). The API provides access to all core functionality through well-defined functions and data structures.

**Clock Configuration:** 16 MHz HSE → 84 MHz SYSCLK  
**Version:** 2.1.0  
**Date:** 2026-03-21

---

## Clock System API

### SystemClock_Config()

Configures the system clock tree.

**Prototype:** `static void SystemClock_Config(void)`

**Description:**
Initializes the complete clock system:
- HSE: 16 MHz external crystal
- PLL: M=16, N=336, P=4, Q=7
- SYSCLK: 84 MHz
- HCLK: 84 MHz
- PCLK1: 42 MHz (APB1)
- PCLK2: 84 MHz (APB2)

**Clock Tree:**
```
HSE (16 MHz)
  └── PLL
      ├── SYSCLK = 84 MHz (336/4)
      ├── HCLK   = 84 MHz
      ├── PCLK1  = 42 MHz (HCLK/2)
      ├── PCLK2  = 84 MHz (HCLK/1)
      └── USB    = 48 MHz (336/7)
```

**Notes:**
- Called automatically in `main()` before peripheral initialization
- Uses FLASH_LATENCY_2 for 84 MHz operation
- Configures voltage scale 2 for power efficiency

---

## Data Structures

### Adaptive_PWM_t

Represents the PWM controller state.

```c
typedef struct {
    TIM_HandleTypeDef htim;      // Timer handle (TIM1)
    uint32_t period;             // ARR value (4199 for 20kHz @ 84MHz)
    uint32_t frequency;          // PWM frequency in Hz
    uint16_t current_duty;       // Current duty cycle value
    bool is_running;             // PWM running state
} Adaptive_PWM_t;
```

**Clock Dependency:** TIM1 runs on APB2 at 84 MHz

### Adaptive_ADC_t

Represents the ADC controller state.

```c
typedef struct {
    ADC_HandleTypeDef hadc;      // ADC handle (ADC1)
    DMA_HandleTypeDef hdma;      // DMA handle (DMA2 Stream 0)
    uint16_t dma_buffer[ADC_DMA_BUFFER_SIZE];  // Raw samples
    ADC_Measurement_t current;     // Current measurement
    ADC_Measurement_t averaged;    // Averaged measurement (IIR filter)
    uint32_t sample_count;       // Total samples processed
    bool conversion_complete;    // DMA complete flag
    bool current_valid;          // Measurement valid flag
} Adaptive_ADC_t;
```

**Clock Dependency:** ADC runs on PCLK2/2 = 42 MHz (maximum)

### ADC_Measurement_t

Contains measured electrical parameters.

```c
typedef struct {
    float vin;           // Input voltage (V)
    float vout;          // Output voltage (V)
    float current;       // Current (A)
    float temperature;   // Temperature (°C)
    uint32_t timestamp;  // Measurement time (ms)
    bool valid;          // Data valid flag
} ADC_Measurement_t;
```

### PWM_Config_t

PWM configuration structure.

```c
typedef struct {
    float duty_cycle_min;
    float duty_cycle_max;
    float target_efficiency;
    uint32_t sample_rate_ms;
    bool secure_mode_enabled;
} pwm_config_t;
```

---

## PWM Functions

### Adaptive_PWM_Init()

Initializes the PWM hardware.

**Prototype:** `bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm)`

**Parameters:**
- `pwm`: Pointer to PWM structure

**Returns:**
- `true`: Initialization successful
- `false`: Initialization failed

**Configuration:**
- Timer: TIM1 (APB2, 84 MHz)
- Frequency: 20 kHz
- ARR: 4199 (84MHz/20kHz - 1)
- Dead-time: 400 ns
- Channels: CH1 (PA8), CH1N (PA9)

**Example:**
```c
Adaptive_PWM_t pwm_handle;
if (!Adaptive_PWM_Init(&pwm_handle)) {
    Error_Handler();
}
```

---

### Adaptive_PWM_Start()

Starts PWM generation.

**Prototype:** `bool Adaptive_PWM_Start(Adaptive_PWM_t* pwm)`

**Returns:**
- `true`: PWM started
- `false`: Already running or error

---

### Adaptive_PWM_Stop()

Stops PWM generation.

**Prototype:** `bool Adaptive_PWM_Stop(Adaptive_PWM_t* pwm)`

**Returns:**
- `true`: PWM stopped
- `false`: Not running or error

---

### Adaptive_PWM_SetDuty()

Sets PWM duty cycle.

**Prototype:** `bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty)`

**Parameters:**
- `pwm`: PWM handle
- `duty`: Duty cycle (0.0 - 1.0, clamped to 0.02-0.98)

**Returns:**
- `true`: Duty cycle set
- `false`: PWM not running

**Safety:**
- Hard limits: 2% - 98%
- Soft limits: 5% - 95%
- Errors logged on limit violations

**Clock Impact:** Immediate register update at next timer tick (11.9 ns resolution)

---

### Adaptive_PWM_GetDuty()

Gets current duty cycle.

**Prototype:** `float Adaptive_PWM_GetDuty(const Adaptive_PWM_t* pwm)`

**Returns:** Current duty cycle (0.0 - 1.0)

---

### Adaptive_PWM_EmergencyStop()

Immediate PWM shutdown via break input.

**Prototype:** `void Adaptive_PWM_EmergencyStop(Adaptive_PWM_t* pwm)`

**Description:**
- Forces outputs to inactive state
- Sets duty cycle to 0
- Requires manual restart

---

## ADC Functions

### Adaptive_ADC_Init()

Initializes ADC with DMA.

**Prototype:** `bool Adaptive_ADC_Init(Adaptive_ADC_t* adc)`

**Parameters:**
- `adc`: Pointer to ADC structure

**Returns:**
- `true`: Initialization successful
- `false`: Initialization failed

**Configuration:**
- ADC: ADC1
- Clock: PCLK2/2 = 42 MHz (maximum)
- Resolution: 12-bit
- Mode: DMA circular, scan mode
- Channels: 4 (Vin, Vout, Current, Temperature)

**Sampling Times (optimized for 42 MHz):**
| Channel | Signal | Cycles | Time |
|---------|--------|--------|------|
| 0 | Vin | 3 | 71 ns |
| 1 | Vout | 3 | 71 ns |
| 2 | Current | 15 | 357 ns |
| 3 | Temperature | 28 | 667 ns |

**Example:**
```c
Adaptive_ADC_t adc_handle;
if (!Adaptive_ADC_Init(&adc_handle)) {
    Error_Handler();
}
```

---

### Adaptive_ADC_Start_DMA()

Starts continuous DMA conversions.

**Prototype:** `bool Adaptive_ADC_Start_DMA(Adaptive_ADC_t* adc)`

**Returns:**
- `true`: DMA started
- `false`: Error

---

### Adaptive_ADC_Stop_DMA()

Stops DMA conversions.

**Prototype:** `bool Adaptive_ADC_Stop_DMA(Adaptive_ADC_t* adc)`

**Returns:**
- `true`: DMA stopped
- `false`: Error

---

### Adaptive_ADC_ProcessBuffer()

Processes DMA buffer data.

**Prototype:** `void Adaptive_ADC_ProcessBuffer(Adaptive_ADC_t* adc)`

**Description:**
- Called from DMA half/complete interrupt
- Averages samples and applies IIR filter
- Updates `current` and `averaged` measurements

**Clock Considerations:**
- Processing time: ~10-50 µs at 84 MHz
- Called at DMA buffer rate (configurable)

---

### Adaptive_ADC_GetMeasurement()

Gets current measurement.

**Prototype:** `bool Adaptive_ADC_GetMeasurement(Adaptive_ADC_t* adc, ADC_Measurement_t* meas)`

**Parameters:**
- `adc`: ADC handle
- `meas`: Output measurement structure

**Returns:**
- `true`: Valid measurement returned
- `false`: No valid data available

---

### Adaptive_ADC_GetAveraged()

Gets averaged measurement.

**Prototype:** `bool Adaptive_ADC_GetAveraged(Adaptive_ADC_t* adc, ADC_Measurement_t* meas)`

**Description:**
Returns IIR-filtered values for noise reduction.

---

### Adaptive_ADC_IsReady()

Checks if new measurement is available.

**Prototype:** `bool Adaptive_ADC_IsReady(Adaptive_ADC_t* adc)`

**Returns:**
- `true`: New conversion complete
- `false`: No new data

---

### Adaptive_ADC_Calibrate()

Calibrates ADC with known values.

**Prototype:** `bool Adaptive_ADC_Calibrate(Adaptive_ADC_t* adc, float known_vin, float known_vout, float known_current)`

**Parameters:**
- `adc`: ADC handle
- `known_vin`: Known input voltage
- `known_vout`: Known output voltage
- `known_current`: Known current

---

## Clock Utility Functions

### HAL_RCC_GetSysClockFreq()

Returns system clock frequency.

**Prototype:** `uint32_t HAL_RCC_GetSysClockFreq(void)`

**Returns:** 84000000 (84 MHz)

---

### HAL_RCC_GetHCLKFreq()

Returns AHB clock frequency.

**Prototype:** `uint32_t HAL_RCC_GetHCLKFreq(void)`

**Returns:** 84000000 (84 MHz)

---

### HAL_RCC_GetPCLK1Freq()

Returns APB1 clock frequency.

**Prototype:** `uint32_t HAL_RCC_GetPCLK1Freq(void)`

**Returns:** 42000000 (42 MHz)

---

### HAL_RCC_GetPCLK2Freq()

Returns APB2 clock frequency.

**Prototype:** `uint32_t HAL_RCC_GetPCLK2Freq(void)`

**Returns:** 84000000 (84 MHz)

---

## Error Handling

### Error_Report()

Reports an error condition.

**Prototype:** `void Error_Report(ErrorManager_t* mgr, ErrorCode_t code, ErrorSeverity_t severity, const char* msg, uint32_t data)`

**Severity Levels:**
- `SEVERITY_INFO`: Informational
- `SEVERITY_WARNING`: Warning, operation continues
- `SEVERITY_ERROR`: Error, limited operation
- `SEVERITY_CRITICAL`: Critical, system shutdown

---

### Error_Critical()

Handles critical errors with system halt.

**Prototype:** `void Error_Critical(ErrorManager_t* mgr, ErrorCode_t code, const char* msg)`

**Behavior:**
- Logs error
- Enters infinite loop
- Watchdog will reset system

---

## Constants

### Clock Constants

```c
#define HSE_FREQ_HZ          16000000UL  // 16 MHz
#define SYSCLK_FREQ_HZ       84000000UL  // 84 MHz
#define HCLK_FREQ_HZ         84000000UL  // 84 MHz
#define PCLK1_FREQ_HZ        42000000UL  // 42 MHz
#define PCLK2_FREQ_HZ        84000000UL  // 84 MHz
```

### PLL Configuration

```c
#define PLLM_VALUE           16
#define PLLN_VALUE           336
#define PLLP_VALUE           4
#define PLLQ_VALUE           7
```

### PWM Limits

```c
#define PWM_HARD_MIN_DUTY    0.02f   // 2%
#define PWM_HARD_MAX_DUTY    0.98f   // 98%
#define PWM_SOFT_MIN_DUTY    0.05f   // 5%
#define PWM_SOFT_MAX_DUTY    0.95f   // 95%
#define PWM_FREQUENCY_HZ      20000    // 20 kHz
```

### ADC Configuration

```c
#define ADC_CLOCK_HZ         42000000UL  // 42 MHz
#define ADC_SAMPLE_RATE_HZ   10000
#define ADC_NUM_CHANNELS     4
#define ADC_DMA_BUFFER_SIZE  64
```

---

## Example Usage

```c
#include "config.h"
#include "hal_pwm.h"
#include "hal_adc.h"

Adaptive_PWM_t pwm;
Adaptive_ADC_t adc;
ADC_Measurement_t meas;

int main(void) {
    HAL_Init();
    SystemClock_Config();  // 16 MHz → 84 MHz
    
    // Initialize peripherals
    Adaptive_PWM_Init(&pwm);
    Adaptive_ADC_Init(&adc);
    
    // Start systems
    Adaptive_PWM_Start(&pwm);
    Adaptive_ADC_Start_DMA(&adc);
    
    // Main loop
    while (1) {
        if (Adaptive_ADC_IsReady(&adc)) {
            Adaptive_ADC_GetMeasurement(&adc, &meas);
            
            // Process measurement
            float efficiency = CalculateEfficiency(&meas);
            
            // Adjust PWM
            float duty = CalculateNewDuty(efficiency);
            Adaptive_PWM_SetDuty(&pwm, duty);
        }
        
        HAL_Delay(1);  // 1 ms loop
    }
}
```

---

## Thread Safety

All API functions are **not thread-safe** by default. When using with FreeRTOS:

- Use mutexes for PWM/ADC shared access
- DMA callbacks run in interrupt context
- Use `portENTER_CRITICAL()` for critical sections

---

## Performance Considerations

### Clock-Sensitive Operations

| Operation | Clock | Typical Time |
|-----------|-------|--------------|
| PWM update | 84 MHz | <100 ns |
| ADC sample | 42 MHz | 1.95 µs (4 ch) |
| DMA transfer | AHB | <1 µs |
| Context switch | 84 MHz | 12 µs |

### Optimization Tips

1. **Use CCM RAM** for critical data (not implemented yet)
2. **Enable I-cache** for flash performance
3. **Align DMA buffers** to 32-byte boundaries
4. **Minimize floating-point** in ISRs

---

## References

- STM32F4 HAL User Manual
- STM32F401 Reference Manual (RM0368)
- FreeRTOS API Reference
