# AdaptivePWM Technical Documentation

## Overview

AdaptivePWM is a real-time control system designed for safe and efficient operation of buck/boost converters and electronic speed controllers (ESCs). The system continuously monitors electrical parameters and dynamically adjusts PWM output to maintain optimal efficiency while ensuring hardware safety.

**Version:** 2.1.0  
**Target:** STM32F401RE @ 84 MHz  
**Clock:** 16 MHz HSE → 84 MHz SYSCLK  
**Framework:** STM32Cube HAL + FreeRTOS

---

## Table of Contents

1. [Clock System Design](design.md) - Complete clock tree documentation
2. [API Reference](api.md) - Function and data structure documentation
3. [Safety Protocols](safety.md) - Safety features and limits
4. [Mål (Goals)](mål.md) - Project goals (Swedish)
5. [Metoder (Methods)](metoder.md) - Implementation methods (Swedish)

---

## System Architecture

The system consists of several key components:

### 1. Clock Management (v2.1.0)

**Clock Tree:**
```
HSE (16 MHz) → PLL → SYSCLK = 84 MHz
├── AHB (HCLK) = 84 MHz
├── APB1 (PCLK1) = 42 MHz  ← ADC, UART, TIM2-5
└── APB2 (PCLK2) = 84 MHz  ← TIM1 PWM, ADC
```

**PLL Configuration:**
- PLLM = 16 (VCO input = 1 MHz)
- PLLN = 336 (VCO output = 336 MHz)
- PLLP = 4 (SYSCLK = 84 MHz)
- PLLQ = 7 (USB = 48 MHz)

See [design.md](design.md) for complete clock documentation.

### 2. Parameter Measurement Module

Reads and processes ADC values to determine:
- **Vin**: Input voltage (PA0)
- **Vout**: Output voltage (PA1)
- **Current**: Current sense (PA2)
- **Temperature**: Thermal monitoring (PA3)

**ADC Configuration:**
- Clock: 42 MHz (maximum allowed)
- Resolution: 12-bit
- Sampling: Optimized per channel (3/15/28 cycles)
- Mode: DMA circular buffer

### 3. PWM Controller

**TIM1 Configuration:**
- Clock: 84 MHz (APB2)
- Frequency: 20 kHz
- Resolution: 4200 steps (~12-bit)
- Dead-time: 400 ns
- Channels: Complementary CH1/CH1N (PA8/PA9)

### 4. Efficiency Calculator

Computes real-time efficiency:
```
Efficiency = 1 - (Switching Losses + Conduction Losses)
Switching Losses = fsw × L × D²
Conduction Losses = ESR × I²
```

### 5. Duty Cycle Controller

Proportional control with limits:
```
ΔDuty = (Target Eff - Actual Eff) × Gain
New Duty = Current Duty + ΔDuty
Clamped: 5% ≤ Duty ≤ 95%
```

### 6. Safety Manager

- Independent watchdog (IWDG)
- Temperature monitoring with derating
- Overcurrent detection
- Emergency PWM shutdown

---

## Key Features

### Clock System (v2.1.0)
- **HSE:** 16 MHz external crystal
- **SYSCLK:** 84 MHz (maximum CPU frequency)
- **ADC Clock:** 42 MHz (PCLK2/2, maximum)
- **PWM Clock:** 84 MHz (full resolution)
- **Latency:** <50 µs (ADC → PWM)

### ADC Performance
- **Sample Rate:** 10 kHz per channel (40 kHz total)
- **Resolution:** 12-bit (4096 levels)
- **Conversion Time:** 1.95 µs (4 channels)
- **Clock:** 42 MHz (maximum for STM32F4)

### PWM Performance
- **Frequency:** 20 kHz
- **Resolution:** 4200 steps (12-bit equivalent)
- **Dead-time:** 400 ns (configurable)
- **Jitter:** <10 ns (hardware)

### Control Loop
- **Update Rate:** 100 Hz
- **Latency:** <50 µs (ADC → PWM)
- **Settling Time:** <10 ms (typical)

---

## File Structure

```
src/
├── main.c                 # Entry point + clock config
├── config.h              # Central configuration
├── hal_pwm.h/c           # PWM driver (84 MHz)
├── hal_adc.h/c           # ADC driver (42 MHz)
├── hal_uart.h/c          # UART driver
├── hal_watchdog.h/c      # Watchdog driver
├── param_calc.h/c        # L/C/ESR calculations
├── freertos_tasks.h/c    # RTOS tasks
├── error_handler.h/c     # Error management
├── temperature_monitor.h/c # Thermal management
├── cli_commands.h/c      # CLI implementation
├── flash_logger.h/c      # Data logging
└── calibration.h/c       # Calibration routines

docs/
├── index.md              # This file
├── design.md             # Clock system design
├── api.md                # API reference
├── safety.md             # Safety protocols
├── mål.md                # Project goals (Swedish)
└── metoder.md            # Methods (Swedish)
```

---

## Hardware Requirements

### STM32F401RE Nucleo Board
- **Core:** ARM Cortex-M4 @ 84 MHz
- **Flash:** 512 KB
- **RAM:** 96 KB
- **Package:** LQFP64

### External Components
- **Crystal:** 16 MHz HSE
- **Load Caps:** Per crystal datasheet (8-20 pF typical)
- **Current Sense:** 10 mΩ shunt resistor
- **Power Supply:** 3.3V

### Pinout

| Signal | Pin | Function | Clock |
|--------|-----|----------|-------|
| PWM_CH1 | PA8 | Main PWM output | 84 MHz |
| PWM_CH1N | PA9 | Complementary PWM | 84 MHz |
| ADC_VIN | PA0 | Input voltage | 42 MHz |
| ADC_VOUT | PA1 | Output voltage | 42 MHz |
| ADC_I | PA2 | Current sense | 42 MHz |
| ADC_T | PA3 | Temperature | 42 MHz |
| UART_TX | PA2 | Serial TX | 42 MHz |
| UART_RX | PA3 | Serial RX | 42 MHz |

---

## Building

### Requirements
- PlatformIO Core
- STM32Cube HAL Framework

### Build Commands
```bash
# Release build (optimized)
pio run -e nucleo_f401re

# Debug build with CLI
pio run -e nucleo_f401re_debug

# Profile build
pio run -e nucleo_f401re_profile

# Upload
pio run --target upload

# Monitor serial output
pio device monitor -b 115200
```

---

## API Overview

See [api.md](api.md) for complete reference.

### Core Functions

```c
// Initialize system with clock config
bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm);
bool Adaptive_ADC_Init(Adaptive_ADC_t* adc);

// Control functions
bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty);
float Adaptive_PWM_GetDuty(const Adaptive_PWM_t* pwm);

// Measurement
void Adaptive_ADC_ProcessBuffer(Adaptive_ADC_t* adc);
bool Adaptive_ADC_GetMeasurement(Adaptive_ADC_t* adc, ADC_Measurement_t* meas);

// Safety
void Adaptive_PWM_EmergencyStop(Adaptive_PWM_t* pwm);
```

---

## Safety Features

See [safety.md](safety.md) for complete documentation.

### Hardware Protection
- Watchdog timer: 500ms timeout
- PWM dead-time: 400ns
- Break input (fault detection)
- Duty cycle limits: 5% - 95%

### Software Protection
- Temperature derating curve
- Overcurrent detection
- Parameter validation
- Emergency shutdown

### Clock Safety
- CSS (Clock Security System)
- HSE failure detection
- Automatic fallback to HSI

---

## Changelog

### v2.1.0 (2026-03-21)
- **Clock System:** Optimized 16 MHz HSE configuration
  - SYSCLK: 84 MHz
  - ADC Clock: 42 MHz (maximum)
  - PWM Clock: 84 MHz (full resolution)
- **ADC:** Optimized sampling times per channel
- **Documentation:** Complete clock system design document

### v2.0.0
- Initial release with FreeRTOS
- Basic PWM and ADC drivers
- CLI interface

---

## References

- STM32F401 Reference Manual (RM0368)
- STM32F4xx HAL User Manual
- AN4488: STM32F4 clock configuration
- FreeRTOS Documentation

---

## License

MIT License

## Author

Ola Andersson
