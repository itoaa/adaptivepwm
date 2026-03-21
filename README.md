# AdaptivePWM Documentation

## Project Overview

AdaptivePWM is a real-time control system for buck/boost converters and electronic speed controllers (ESCs). It continuously monitors electrical parameters and dynamically adjusts PWM output for optimal efficiency.

**Version:** 2.1.0  
**Target:** STM32F401RE @ 84 MHz  
**Clock:** 16 MHz HSE → 84 MHz SYSCLK

---

## Clock System (v2.1.0 Optimized)

### Configuration
```
HSE: 16 MHz external crystal
PLL: M=16, N=336, P=4, Q=7
SYSCLK: 84 MHz (maximum)
HCLK: 84 MHz
PCLK1: 42 MHz (APB1 - ADC, UART)
PCLK2: 84 MHz (APB2 - TIM1 PWM)
```

### Optimizations
- **ADC Clock:** 42 MHz (maximum, PCLK2/2)
- **PWM Clock:** 84 MHz (full resolution)
- **Sampling:** Optimized per channel (3/15/28 cycles)
- **Latency:** <50 µs (ADC → PWM)

See [docs/design.md](docs/design.md) for complete clock documentation.

---

## Features

### Implemented Components

1. **PWM Hardware Abstraction** (`hal_pwm.h/c`)
   - TIM1 complementary PWM with dead-time
   - Frequency: 20kHz @ 84 MHz clock
   - Duty cycle: 5% - 95% with hardware limits
   - Emergency stop via break input

2. **ADC Hardware Abstraction** (`hal_adc.h/c`)
   - 4-channel DMA-based sampling
   - Clock: 42 MHz (maximum)
   - Channels: Vin, Vout, Current, Temperature
   - Optimized sampling times per channel
   - Sample rate: 10kHz total

3. **Parameter Calculation** (`param_calc.h/c`)
   - Real-time L, C, ESR calculation
   - Formulas:
     - L = (Vin - Vout) × D / (fsw × ΔI)
     - C = ΔI × D / (fsw × ΔV)
     - ESR = ΔV / ΔI (simplified)

4. **FreeRTOS Tasks** (`freertos_tasks.h/c`)
   - Measurement task: 1kHz
   - Control task: 100Hz
   - Safety task: 100Hz (highest priority)
   - CLI task: 50Hz

5. **Safety Systems**
   - Independent watchdog (IWDG)
   - Temperature monitoring with derating
   - Overcurrent detection
   - Emergency PWM shutdown

6. **CLI Interface** (`cli_commands.h/c`)
   - Commands: status, config, monitor, pwm, calibrate, errors, help
   - UART: 115200 baud
   - Interrupt-driven RX

7. **Error Handler** (`error_handler.h/c`)
   - 4 severity levels: INFO, WARNING, ERROR, CRITICAL
   - Circular log buffer (16 entries)
   - Automatic shutdown on critical errors

8. **Flash Logger** (`flash_logger.h/c`)
   - Persistent data logging
   - Circular buffer in flash sector 11
   - CRC validation

9. **Temperature Monitor** (`temperature_monitor.h/c`)
   - Thermal derating curve
   - Warning: 75°C
   - Critical: 85°C
   - Shutdown: 95°C

10. **Calibration** (`calibration.h/c`)
    - Automatic gain/offset calibration
    - Flash storage

---

## File Structure

```
src/
├── main.c                 # Entry point (clock config)
├── hal_pwm.h/c           # PWM driver (84 MHz)
├── hal_adc.h/c           # ADC driver (42 MHz, optimized)
├── hal_uart.h/c          # UART driver
├── hal_watchdog.h/c      # Watchdog driver
├── param_calc.h/c        # L/C/ESR calculations
├── freertos_tasks.h/c    # RTOS tasks
├── error_handler.h/c     # Error management
├── temperature_monitor.h/c # Thermal management
├── cli_commands.h/c      # CLI implementation
├── flash_logger.h/c      # Data logging
├── calibration.h/c       # Calibration routines
├── current_protection.h  # Overcurrent protection
└── config.h              # Central configuration (clock settings)

include/
└── pwm_cli.h             # CLI interface header

cli/
├── pwm_cli.py            # Python CLI tool
├── pwmctl.c              # C CLI implementation
└── config.json           # CLI configuration

docs/
├── index.md              # Documentation index
├── design.md             # Clock system design ⭐
├── api.md                # API reference
├── safety.md             # Safety protocols
└── (goal/method files in Swedish)

test/
└── test_adaptivepwm.py   # Python unit tests
```

---

## Building

### Requirements
- PlatformIO
- STM32 CubeMX HAL

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

## Usage

### Hardware Connections

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

### Crystal Requirements
- **Frequency:** 16 MHz
- **Type:** HSE (High Speed External)
- **Accuracy:** ±20 ppm recommended
- **Load capacitance:** As per crystal datasheet (typically 8-20 pF)

### CLI Commands

```
status [adc|pwm|params]  - Show system status
config <param> <value>   - Configure system
monitor [duration]         - Real-time monitoring
pwm <duty|start|stop>      - PWM control
calibrate <vin> <vout>   - Calibrate ADC
errors [clear]            - Show/clear error log
help [command]           - Show help
```

---

## Safety Features

### Hardware Protection
- Watchdog timer: 500ms timeout
- PWM dead-time: 400ns
- Break input (fault detection)
- Duty cycle limits: 5% - 95%

### Software Protection
- Temperature derating
- Overcurrent detection
- Parameter validation
- Emergency shutdown

### Clock Safety
- CSS (Clock Security System) enabled
- HSE failure detection
- Automatic fallback to HSI

---

## Performance

### ADC Performance
- **Clock:** 42 MHz (maximum)
- **Resolution:** 12-bit
- **Sample Rate:** 10 kHz per channel
- **Conversion Time:** 1.95 µs (4 channels)

### PWM Performance
- **Clock:** 84 MHz
- **Frequency:** 20 kHz
- **Resolution:** 4200 steps (12-bit equivalent)
- **Dead-time:** 400 ns

### Control Loop
- **Update Rate:** 100 Hz
- **Latency:** <50 µs (ADC → PWM)
- **Jitter:** <10 ns (hardware)

---

## Algorithms

### Efficiency Calculation
```
Efficiency = 1 - (Switching Losses + Conduction Losses)
Switching Losses = fsw × L × D²
Conduction Losses = ESR × I²
```

### Duty Cycle Control
```
ΔDuty = (Target Eff - Actual Eff) × Gain
New Duty = Current Duty + ΔDuty
Clamped: 5% ≤ Duty ≤ 95%
```

---

## Testing

### Unit Tests
```bash
cd test
python3 -m pytest test_adaptivepwm.py -v
```

### Hardware Tests
1. Clock verification (scope on MCO pin)
2. PWM output verification (scope)
3. ADC accuracy test (known voltage sources)
4. Parameter calculation validation
5. Safety system response test

---

## Changelog

### v2.1.0 (2026-03-21)
- **Clock System:** Optimized for 16 MHz HSE
- **ADC:** Maximum 42 MHz clock, optimized sampling times
- **PWM:** Full 84 MHz resolution
- **Documentation:** Complete clock system design document

### v2.0.0
- Initial release with FreeRTOS
- Basic PWM and ADC drivers
- CLI interface

---

## Future Enhancements

- [ ] CAN bus communication
- [ ] Machine learning optimization
- [ ] Advanced predictive algorithms
- [ ] Multi-channel support
- [ ] Ethernet connectivity
- [ ] ADC-PWM hardware synchronization

---

## License

MIT License

## Author

Ola Andersson
