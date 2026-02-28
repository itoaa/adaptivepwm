# AdaptivePWM Technical Documentation

## Overview

AdaptivePWM is a real-time control system designed for safe and efficient operation of buck/boost converters and electronic speed controllers (ESCs). The system continuously monitors electrical parameters and dynamically adjusts PWM output to maintain optimal efficiency while ensuring hardware safety.

## System Architecture

The system consists of several key components:

1. **Parameter Measurement Module**: Reads and processes ADC values to determine L, C, and ESR
2. **Efficiency Calculator**: Computes real-time efficiency based on measured parameters
3. **Duty Cycle Controller**: Adjusts PWM output to optimize efficiency within safe limits
4. **Safety Manager**: Monitors system health and implements failover procedures

## Key Algorithms

### Electrical Parameter Measurement

The system measures three critical electrical parameters:

- **Inductance (L)**: Measured in millihenries using ADC readings from current sensors
- **Capacitance (C)**: Measured in microfarads using voltage ripple analysis
- **Equivalent Series Resistance (ESR)**: Measured in milliohms using impedance calculations

These measurements are averaged over 16 samples to reduce noise and improve accuracy.

### Efficiency Calculation

Efficiency is calculated using the formula:
```
Efficiency = 1 - (Switching Losses + Conduction Losses) / Input Power
```

Where:
- Switching Losses = fsw × L × D²
- Conduction Losses = ESR × I²

### Duty Cycle Control

The duty cycle is adjusted using a proportional controller:
```
New Duty Cycle = Current Duty Cycle + (Target Efficiency - Actual Efficiency) × Gain
```

Limits are enforced to keep the duty cycle between 5% and 95%.

## Safety Features

1. **Parameter Validation**: All measurements are validated against expected ranges
2. **Boundary Checking**: Duty cycle and all electrical parameters have safe limits
3. **Fail-Safe Mode**: System defaults to 50% duty cycle on measurement failure
4. **Watchdog Timer**: Regular health checks ensure system responsiveness

## Hardware Integration

### ADC Configuration

The system requires properly configured ADC channels for:
- Voltage sensing
- Current sensing
- Temperature monitoring (optional)

### PWM Output

PWM signals are generated using STM32 timers with:
- Variable frequency support
- Dead time insertion for H-bridge configurations
- Emergency stop capability

## API Reference

See header files for detailed function descriptions:
- `AdaptivePWM.h`: Main interface definitions
- `main.c`: Implementation details

## Troubleshooting

Common issues and solutions:
1. **ADC Read Failures**: Check sensor connections and power supply
2. **Efficiency Below Target**: Verify component ratings and thermal conditions
3. **Communication Errors**: Ensure proper grounding and shielding

## Future Enhancements

Planned improvements include:
- FreeRTOS integration for multitasking
- CAN bus communication for multi-unit systems
- Advanced predictive algorithms using machine learning