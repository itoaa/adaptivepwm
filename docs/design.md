# AdaptivePWM — Design

**Version:** 2.5.x (bring-up)  
**MCU:** STM32F401RE @ 84 MHz (NUCLEO-F401RE)  
**Källträd:** `App/`, `Core/`, `config/`  
**Status:** se [MATURITY.md](../MATURITY.md)

## 1. Syfte

Realtids **PWM-styrning** av buck/boost-liknande effektsteg:

- Mäta Vin, Vout, ström, temperatur  
- Styra TIM1-PWM med säkra duty-gränser  
- **Reglera Vout** mot setpoint (PID) i bring-up  
- Hålla safe operating area (ström/spänning/temp) med mjuk fault-latch  
- Lab-CLI över UART  

Sekundärt (av default): L/C/ESR från switch-ripple, η-baserad duty, security-profil.

---

## 2. Systemöversikt

```text
                    ┌──────────────────────────────────────┐
                    │           main() init                │
                    │  clock, WDG, PWM/ADC/UART, CLI banner│
                    └──────────────────┬───────────────────┘
                                       │
                                       ▼
                    ┌──────────────────────────────────────┐
                    │   Bare-metal superloop (default)     │
                    │   Tasks_StartScheduler()             │
                    │                                      │
                    │  ~1 kHz  Loop_Measure                │
                    │  ~100 Hz Loop_Control  (Vout PID)    │
                    │  ~100 Hz Loop_Safety   (limits)      │
                    │  ~50 Hz  Loop_CLI      (commands)    │
                    └──────────────────────────────────────┘
                          │              │
          ┌───────────────┼──────────────┼───────────────┐
          ▼               ▼              ▼               ▼
     ADC1+DMA          TIM1 PWM      IWDG + latch      USART2
     Vin Vout I T      dead-time     emergency stop    CLI RX buffer
```

**Default runtime är inte FreeRTOS.** Filen `freertos_tasks.c` kör en **kooperativ superloop** när `USE_FREERTOS` inte är definierad (CI/CubeIDE-bygget). FreeRTOS-path finns som villkorlig kod om flaggan sätts senare.

---

## 3. Klocka

| Parameter | Värde |
|-----------|--------|
| HSE | 16 MHz (extern) |
| PLL | M=16, N=336, P=4, Q=7 |
| SYSCLK / HCLK | 84 MHz |
| PCLK1 (APB1) | 42 MHz — UART, IWDG-relaterat |
| PCLK2 (APB2) | 84 MHz — TIM1, ADC-klocka |
| ADC-klocka | PCLK2/2 = 42 MHz |
| PWM TIM1 | 84 MHz timer-klocka, typiskt **20 kHz** PWM (`PWM_FREQUENCY_HZ`) |

Konfiguration: `App/Src/main.c` → `SystemClock_Config()`, konstanter i `App/Inc/config.h`.

---

## 4. Dataflöde (bring-up)

```text
DMA complete (ISR flag)
        │
        ▼
Loop_Measure
  Adaptive_ADC_ProcessBuffer()     // rå → Vin/Vout/I/T + filter
  ParamCalc_AddSample / CalculateAll
        │  averages_valid (L/C endast om FEATURE_SWITCH_RIPPLE_ESTIMATION)
        ▼
Loop_Control  (om PWM running, ej fault, FEATURE_VOUT_CONTROL)
  mät Vout (medel)
  PID_Compute(setpoint, Vout, dt) → duty
  Adaptive_PWM_SetDuty()           // ramp + soft/hard limits
        │
Loop_Safety
  OV / OC / OT / (valfri UV)
  vid fel: EmergencyStop + latch
  soft recovery efter cooldown (PWM startas manuellt igen)
        │
Loop_CLI
  läs färdig UART-rad (ej tungt i IRQ)
  CLI_ProcessCommand()
  async monitor-rader
```

### UART / IRQ-regel

- **ISR:** endast RX-buffer (`Adaptive_UART_ProcessRX`).  
- **CLI / monitor / printf:** i superloop — aldrig blockera i avbrott.

---

## 5. Reglering

### 5.1 Primär: Vout-PID (`FEATURE_VOUT_CONTROL=1`)

| Parameter | Källa |
|-----------|--------|
| Setpoint | `VOUT_SETPOINT_DEFAULT_V` / `Tasks_SetVoutSetpoint()` / CLI `config vout` |
| Mätning | filtrerad `meas.vout` |
| Utgång | duty ∈ soft min…max |
| PID-state | per-instans (`d_filtered` i struct) |

Gains: `DUTY_KP/KI/KD`, setpoint weight, derivative filter i `config.h`.

### 5.2 Avstängt som default

| Mode | Flagga | Kommentar |
|------|--------|-----------|
| η → duty | `FEATURE_EFFICIENCY_CONTROL` | Kräver giltig Pin/Pout; nuvarande en-ströms-skattning är otillräcklig |
| L/C/ESR | `FEATURE_SWITCH_RIPPLE_ESTIMATION` | Kräver sampling ≫ 20 kHz; host-path ~1 kHz räcker inte |

### 5.3 Open-loop

Om båda control-flaggor är 0: duty styrs bara via CLI (`pwm <%>`).

---

## 6. Mätning (ADC)

| Kanal | Signal | Typisk pin (Nucleo-layout) |
|-------|--------|----------------------------|
| 0 | Vin | PA0 |
| 1 | Vout | PA1 |
| 2 | Current | PA2 |
| 3 | Temp | PA3 |

- **DMA** cirkulär buffer → process i `Loop_Measure`.  
- Filter: moving average + IIR (konfigurerbart).  
- **Skalning** (bring-up default = 1.0 för pin-volt-domän):

```text
V_actual = V_adc_pin × ADC_VIN_DIVIDER_RATIO   (motsv. Vout)
I_actual = V_adc_pin / ADC_CURRENT_V_PER_AMP
```

Justera i `App/Inc/config.h` eller via kalibrerings-API för er power-board.

---

## 7. PWM

| Egenskap | Design |
|----------|--------|
| Timer | TIM1 complementary-capable |
| Frekvens | `PWM_FREQUENCY_HZ` (20 kHz) |
| Duty soft | 5–95 % typiskt |
| Duty hard | 2–98 % |
| Ramp | `PWM_RAMP_RATE_PER_SEC` |
| Dead-time | `PWM_DEAD_TIME_NS` |
| Nödstopp | break / `Adaptive_PWM_EmergencyStop` |

---

## 8. Safety

| Lager | Beteende |
|-------|----------|
| Gränser | `CURRENT_MAX_A`, `VOLTAGE_MAX_V`, `TEMP_SHUTDOWN_C`, … |
| VIN UV | `SAFETY_VIN_UV_ENABLE` (default **0** i lab) |
| Vout UV | först efter `SAFETY_VOUT_UV_ENABLE_MS` med PWM igång |
| Fault | latch + PWM stop — **ingen** oändlig hang med IRQ av |
| Recovery | `FEATURE_SOFT_FAULT_RECOVERY`: friska mätvärden + cooldown; `pwm clear` / `pwm start` |

Enhanced safety / fault history-moduler finns i kod; den **aktiva** bring-up-vägen är superloopens `Loop_Safety`.

Watchdog (IWDG) initieras tidigt; refresh i superloop (och SysTick i main).

---

## 9. CLI (kontrakt)

| Kommando | Roll |
|----------|------|
| `help` | Lista |
| `status` / `status adc\|pwm\|params` | Tillstånd |
| `config features` | Feature-flaggor |
| `config vout <V>` | Setpoint |
| `pwm start\|stop\|clear\|<duty%>` | PWM |
| `monitor <s>` | Async CSV (ej block i IRQ) |
| `login` / auth | Endast om `FEATURE_CLI_AUTH` |

Auth **av** default → alla kommandon tillgängliga i lab.

---

## 10. Modul-karta (kod)

| Mapp / fil | Ansvar |
|------------|--------|
| `App/Src/main.c` | Init, klocka, IRQ-handers (lätt) |
| `App/Src/freertos_tasks.c` | Superloop + Loop_* + Vout-PID |
| `App/Src/hal_*.c` | PWM, ADC, UART, WDG |
| `App/Src/param_calc.c` | Medelvärden / ev. L-C |
| `App/Src/cli_*.c` | CLI + valfri auth |
| `App/Src/pid_controller.c` | PID |
| `config/features.h` | Feature switches |
| `App/Inc/config.h` | Klocka, gränser, skalning, gains |
| `Core/` | HAL conf, MSP, syscalls, system |
| `bootloader/` | Signerad start (ofullständig recovery) |

---

## 11. Feature-profil

Se `config/features.h`. Bring-up-default:

- Vout-control **på**  
- Efficiency / switch-ripple / security-profile **av**  
- Soft fault recovery **på**

---

## 12. Icke-mål i denna designrevision

- Inte att FreeRTOS är default.  
- Inte att L/C i realtid är validerat utan snabb sampling.  
- Inte att CISSP/SIL-dokument i `docs/archive/` är implementerade end-to-end.

När HIL är grön uppdateras [MATURITY.md](../MATURITY.md); designen ovan är det som koden faktiskt gör i 2.5 bring-up.
