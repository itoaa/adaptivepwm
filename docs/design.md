# AdaptivePWM Design Document

## System Architecture

### Overview
AdaptivePWM is a real-time control system for buck/boost converters and electronic speed controllers (ESCs). It uses an STM32F401RE microcontroller running at 84 MHz with optimized clock configuration.

## Clock System Design

### Clock Tree (16MHz HSE)

```
                    ┌─────────────────────────────────────┐
                    │           16 MHz HSE                │
                    │     (External Crystal)              │
                    └──────────────┬──────────────────────┘
                                   │
                                   ▼
                    ┌─────────────────────────────────────┐
                    │              PLL                    │
                    │  ┌─────────────────────────────┐  │
                    │  │  PLLM = 16  → VCO in = 1 MHz │  │
                    │  │  PLLN = 336 → VCO out = 336  │  │
                    │  │  PLLP = 4   → SYSCLK = 84    │  │
                    │  │  PLLQ = 7   → USB = 48 MHz  │  │
                    │  └─────────────────────────────┘  │
                    └──────────────┬──────────────────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │              │              │
                    ▼              ▼              ▼
            ┌──────────┐   ┌──────────┐   ┌──────────┐
            │   AHB    │   │   APB1   │   │   APB2   │
            │  84 MHz  │   │  42 MHz  │   │  84 MHz  │
            │  (HCLK)  │   │ (PCLK1)  │   │ (PCLK2)  │
            └────┬─────┘   └────┬─────┘   └────┬─────┘
                 │              │              │
                 │              │              │
                 ▼              ▼              ▼
            ┌──────────┐   ┌──────────┐   ┌──────────┐
            │  Flash   │   │   ADC    │   │   TIM1   │
            │   RAM    │   │  (42MHz) │   │  (84MHz) │
            │   DMA    │   │   UART   │   │   SPI1   │
            │          │   │  TIM2-5  │   │          │
            └──────────┘   └──────────┘   └──────────┘
```

### Clock Configuration Details

| Parameter | Value | Description |
|-----------|-------|-------------|
| HSE | 16 MHz | External crystal oscillator |
| PLLM | 16 | VCO input prescaler (16MHz/16 = 1MHz) |
| PLLN | 336 | VCO multiplier (1MHz × 336 = 336MHz) |
| PLLP | 4 | System clock divider (336/4 = 84MHz) |
| PLLQ | 7 | USB clock divider (336/7 = 48MHz) |
| SYSCLK | 84 MHz | Maximum CPU frequency |
| HCLK | 84 MHz | AHB bus clock |
| PCLK1 | 42 MHz | APB1 bus (max allowed) |
| PCLK2 | 84 MHz | APB2 bus (full speed) |

### Peripheral Clock Distribution

| Peripheral | Bus | Clock | Max Frequency |
|------------|-----|-------|---------------|
| TIM1 (PWM) | APB2 | 84 MHz | 84 MHz |
| TIM2-5 | APB1 | 42 MHz | 42 MHz |
| ADC1 | APB2 | 42 MHz | 42 MHz |
| USART1 | APB2 | 84 MHz | 84 MHz |
| USART2 | APB1 | 42 MHz | 42 MHz |
| SPI1 | APB2 | 84 MHz | 84 MHz |
| SPI2-3 | APB1 | 42 MHz | 42 MHz |
| I2C1-3 | APB1 | 42 MHz | 42 MHz |

## ADC Design

### ADC Clock Optimization

The ADC clock is derived from PCLK2 (84 MHz) divided by 2:
- **ADC Clock = 42 MHz** (maximum allowed for 12-bit resolution)

### Sampling Time Configuration

| Channel | Signal | Sampling Time | Conversion Time | Use Case |
|---------|--------|---------------|-----------------|----------|
| 0 | Vin | 3 cycles | 357 ns | Fast voltage |
| 1 | Vout | 3 cycles | 357 ns | Fast voltage |
| 2 | Current | 15 cycles | 643 ns | Current sense |
| 3 | Temperature | 28 cycles | 952 ns | Slow signal |

**Total cycle time:** 3+3+15+28 + 4×12 = 82 cycles @ 42 MHz = **1.95 µs**

**Maximum sampling rate:** ~500 kHz (4 channels)

### ADC Resolution vs Clock

| Resolution | Max ADC Clock | Min Cycles | Max Sample Rate @ 42MHz |
|------------|---------------|------------|-------------------------|
| 12-bit | 36 MHz | 12 cycles | 3.5 MSPS |
| 10-bit | 42 MHz | 10 cycles | 4.2 MSPS |
| 8-bit | 42 MHz | 8 cycles | 5.25 MSPS |
| 6-bit | 42 MHz | 6 cycles | 7 MSPS |

We use 12-bit with 42 MHz clock (slightly overclocked but stable).

## PWM Design

### TIM1 Configuration

- **Timer Clock:** 84 MHz (APB2)
- **Frequency:** 20 kHz
- **ARR Value:** 4199 (84MHz/20kHz - 1)
- **Resolution:** 4200 steps (~12-bit equivalent)
- **Dead-time:** 400 ns

### PWM Timing

| Parameter | Value | Calculation |
|-----------|-------|-------------|
| Period | 50 µs | 1/20 kHz |
| Tick duration | 11.9 ns | 1/84 MHz |
| ARR | 4199 | 84M/20k - 1 |
| Dead-time ticks | 34 | 400ns / 11.9ns |
| Minimum duty | 2% | 84 ticks |
| Maximum duty | 98% | 4115 ticks |

## Control Loop Timing

### Task Scheduling

| Task | Frequency | Period | Priority |
|------|-----------|--------|----------|
| Safety Monitor | 1 kHz | 1 ms | Highest |
| ADC Processing | 1 kHz | 1 ms | High |
| Control Loop | 100 Hz | 10 ms | Medium |
| CLI Handler | 50 Hz | 20 ms | Low |

### ADC to PWM Latency

| Stage | Time | Notes |
|-------|------|-------|
| ADC Sampling | 1.95 µs | All 4 channels |
| DMA Transfer | <1 µs | Hardware |
| Processing | 10-50 µs | Software filter |
| PWM Update | <1 µs | Register write |
| **Total** | **~50 µs** | **<1% of PWM period** |

## Memory Map

### Flash Layout

| Address | Size | Content |
|---------|------|---------|
| 0x0800 0000 | 64 KB | Bootloader (optional) |
| 0x0801 0000 | 384 KB | Application code |
| 0x0807 0000 | 128 KB | Data logging |

### RAM Layout

| Address | Size | Content |
|---------|------|---------|
| 0x2000 0000 | 128 KB | Main RAM |
| 0x2001 C000 | 16 KB | CCM RAM (fast) |

## Power Consumption

### Clock Tree Power

| Component | Frequency | Typical Current |
|-----------|-----------|-----------------|
| HSE | 16 MHz | ~1 mA |
| PLL | 336 MHz VCO | ~2 mA |
| SYSCLK | 84 MHz | ~10 mA |
| ADC | 42 MHz | ~1.5 mA |
| TIM1 | 84 MHz | ~0.5 mA |
| **Total** | - | **~15 mA @ 3.3V** |

## Performance Metrics

### ADC Performance

- **Sample Rate:** 10 kHz per channel (40 kHz total)
- **Resolution:** 12-bit (4096 levels)
- **ENOB:** ~10.5 bits (estimated)
- **SNR:** ~65 dB (estimated)

### PWM Performance

- **Frequency:** 20 kHz (fixed)
- **Resolution:** 4200 steps (12-bit equivalent)
- **Dead-time:** 400 ns (adjustable)
- **Jitter:** <10 ns (hardware)

### Control Loop Performance

- **Update Rate:** 100 Hz
- **Latency:** <100 µs (ADC to PWM)
- **Settling Time:** <10 ms (typical)

## Safety Features

### Clock Safety

- **CSS (Clock Security System):** Enabled
- **HSE failure detection:** Automatic switch to HSI
- **PLL lock detection:** Hardware monitored

### Watchdog

- **Type:** Independent Watchdog (IWDG)
- **Clock:** 32 kHz LSI
- **Timeout:** 500 ms
- **Refresh:** Every 100 ms

## Future Optimizations

### Potential Clock Improvements

1. **ADC Oversampling:** Use hardware oversampling for better SNR
2. **DMA Double Buffer:** Reduce CPU overhead
3. **Timer Synchronization:** Sync ADC trigger with PWM
4. **CCM RAM:** Move critical data to fast memory

### Alternative Clock Configurations

#### Option 1: Lower Power (48 MHz)
- HSE: 16 MHz
- PLL: M=16, N=192, P=4
- SYSCLK: 48 MHz
- Power: ~40% reduction

#### Option 2: Higher Performance (100 MHz)
- HSE: 25 MHz (requires different crystal)
- PLL: M=25, N=400, P=4
- SYSCLK: 100 MHz
- Note: Requires voltage scale 1

## References

- STM32F401 Reference Manual (RM0368)
- STM32F4xx HAL User Manual
- AN4488: STM32F4 clock configuration
