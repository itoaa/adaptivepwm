# API Documentation

## Overview

This document describes the Application Programming Interface for the AdaptivePWM system. The API provides access to all core functionality through well-defined functions and data structures.

## Data Structures

### AdaptivePWM_State
Represents the current state of the AdaptivePWM system.

```c
typedef struct {
    float L_mH;           // Inductance in millihenries
    float C_uF;           // Capacitance in microfarads
    float ESR_mOhm;       // Equivalent Series Resistance in milliohms
    float duty_cycle;     // Current duty cycle (0.0 - 1.0)
    float efficiency;     // Calculated efficiency (0.0 - 1.0)
    bool initialized;     // System initialization status
} AdaptivePWM_State;
```

### AdaptivePWM_Context
Contains context information for system operation.

```c
typedef struct {
    ADC_HandleTypeDef* hadc;
    TIM_HandleTypeDef* htim;
    uint16_t adc_buffer[ADC_BUFFER_SIZE];
    uint32_t last_measurement_time;
    uint32_t last_adjustment_time;
} AdaptivePWM_Context;
```

## Core Functions

### system_init()
Initializes the entire AdaptivePWM system.

**Prototype**: `bool system_init(AdaptivePWM_Context* ctx)`

**Parameters**: 
- `ctx`: Pointer to context structure

**Returns**: 
- `true` if successful
- `false` if initialization failed

**Description**: 
Initializes HAL, ADC, and timer peripherals. Sets up initial system state.

### measure_electrical_parameters()
Measures electrical parameters using ADC.

**Prototype**: `bool measure_electrical_parameters(AdaptivePWM_Context* ctx, AdaptivePWM_State* state)`

**Parameters**: 
- `ctx`: Pointer to context structure
- `state`: Pointer to state structure for storing results

**Returns**: 
- `true` if measurements are valid
- `false` if measurements are out of range

**Description**: 
Reads ADC values and converts them to electrical parameters. Performs validation on results.

### calculate_efficiency()
Calculates system efficiency based on electrical parameters.

**Prototype**: `float calculate_efficiency(float inductance, float capacitance, float esr)`

**Parameters**: 
- `inductance`: Inductance in mH
- `capacitance`: Capacitance in µF
- `esr`: Equivalent Series Resistance in mΩ

**Returns**: 
Efficiency value between 0.0 and 1.0

**Description**: 
Implements efficiency calculation algorithm considering switching and conduction losses.

### adjust_duty_cycle()
Adjusts PWM duty cycle to optimize efficiency.

**Prototype**: `bool adjust_duty_cycle(AdaptivePWM_Context* ctx, AdaptivePWM_State* state, float target_efficiency)`

**Parameters**: 
- `ctx`: Pointer to context structure
- `state`: Pointer to state structure containing current values
- `target_efficiency`: Desired efficiency (typically 0.95)

**Returns**: 
- `true` if duty cycle was adjusted
- `false` if no adjustment was needed

**Description**: 
Uses proportional control to adjust duty cycle toward target efficiency while respecting limits.

### control_loop()
Main control loop implementing all system functions.

**Prototype**: `void control_loop(AdaptivePWM_Context* ctx, AdaptivePWM_State* state)`

**Parameters**: 
- `ctx`: Pointer to context structure
- `state`: Pointer to state structure

**Description**: 
Coordinates measurement, efficiency calculation, and duty cycle adjustment in a continuous loop.

## Utility Functions

### read_adc_safely()
Reads and averages ADC values for noise reduction.

**Prototype**: `uint16_t read_adc_safely(ADC_HandleTypeDef* hadc)`

**Parameters**: 
- `hadc`: Pointer to ADC handle

**Returns**: 
Averaged ADC value

### init_adc()
Initializes ADC peripheral with appropriate settings.

**Prototype**: `bool init_adc(ADC_HandleTypeDef* hadc)`

**Parameters**: 
- `hadc`: Pointer to ADC handle

**Returns**: 
- `true` if successful
- `false` if initialization failed

### error_handler()
Handles critical system errors with safe shutdown.

**Prototype**: `void error_handler(void)`

**Description**: 
Sets duty cycle to safe minimum and enters infinite loop waiting for reset.

## Constants

### System Limits
```c
#define MAX_DUTY_CYCLE 0.95f      // Maximum allowable duty cycle
#define MIN_DUTY_CYCLE 0.05f      // Minimum allowable duty cycle
#define TARGET_EFFICIENCY 0.95f   // Target system efficiency
#define ADC_BUFFER_SIZE 16        // Number of samples for averaging
```

## Example Usage

```c
AdaptivePWM_Context ctx;
AdaptivePWM_State state;

// Initialize system
if (!system_init(&ctx)) {
    error_handler();
}

// Main loop
while (1) {
    control_loop(&ctx, &state);
    HAL_Delay(10);  // Non-blocking delay
}
```

## Thread Safety

This API is designed for single-threaded bare-metal operation. When integrating with RTOS systems like FreeRTOS, appropriate synchronization primitives should be added.

## Error Codes

The system uses boolean returns for error indication:
- `true`: Success
- `false`: Failure

Detailed error information is logged through the system's error reporting mechanism when available.