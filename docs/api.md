# AdaptivePWM — API (bring-up)

**Version:** 2.5.x  
**Headers:** `App/Inc/*.h`, `config/features.h`  
**Runtime:** bare-metal superloop (`Tasks_StartScheduler`) om inte `USE_FREERTOS`

Detta dokument beskriver de API som **används i nuvarande bring-up-väg**.  
Auth/HMAC/secure-boot-API finns i headers men är **av** via feature-flaggor — se [MATURITY.md](../MATURITY.md).

---

## 1. Runtime / “tasks”

**Header:** `freertos_tasks.h` (namn historiskt; default = superloop)

| Funktion | Beskrivning |
|----------|-------------|
| `bool Tasks_Init(TaskManager_t* manager)` | Init efficiency ctx, Vout-PID, (RTOS-primitiver om FreeRTOS) |
| `void Tasks_StartScheduler(void)` | **Returnerar inte:** superloop eller FreeRTOS scheduler |
| `float Tasks_GetVoutSetpoint(void)` | Aktuell Vout-setpoint (V) |
| `void Tasks_SetVoutSetpoint(float v)` | Sätt setpoint, nollställ PID |
| `bool Tasks_IsSafetyFault(void)` | Soft fault latched? |
| `uint32_t Tasks_GetLastFaultCode(void)` | Senaste fault-kod |
| `void Tasks_ClearSafetyFault(void)` | Rensa latch (PWM startas separat) |
| `void Tasks_StartMonitor(Adaptive_UART_t* uart, int duration_s)` | Async CSV-monitor |
| `bool Tasks_MonitorActive(void)` | Monitor körs? |
| `void Tasks_SuspendControl` / `ResumeControl` | Pausar reglering (control_enabled) |
| `void Tasks_TriggerSafety(TaskManager_t*, uint32_t code)` | Nödstopp + latch |
| `uint32_t Tasks_GetStats(...)` | Statussträng |

**Perioder (superloop):** measure ~1 kHz, control/safety ~100 Hz, CLI ~50 Hz  
(`TASK_FREQ_*` i headern).

---

## 2. PWM

**Header:** `hal_pwm.h`

| Funktion | Beskrivning |
|----------|-------------|
| `bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm)` | TIM1 + GPIO + dead-time |
| `bool Adaptive_PWM_Start` / `Stop` | Start/stopp utgångar |
| `bool Adaptive_PWM_SetDuty(pwm, float duty)` | 0…1 med ramp, soft/hard limits, hysteres |
| `bool Adaptive_PWM_SetDutyImmediate(...)` | Utan ramp |
| `float Adaptive_PWM_GetDuty` / `GetFrequency` | Avläsning |
| `void Adaptive_PWM_EmergencyStop` | Snabb stopp |

**Konstanter:** `PWM_FREQUENCY_HZ`, `PWM_SOFT_*`, `PWM_HARD_*`, `PWM_RAMP_*` i `config.h`.

---

## 3. ADC

**Header:** `hal_adc.h`

```c
typedef struct {
    float vin, vout, current, temperature;
    uint32_t timestamp;
    bool valid;
} ADC_Measurement_t;
```

| Funktion | Beskrivning |
|----------|-------------|
| `bool Adaptive_ADC_Init` | ADC1 + DMA-setup |
| `bool Adaptive_ADC_Start_DMA` / `Stop_DMA` | Kontinuerlig sampling |
| `void Adaptive_ADC_ProcessBuffer` | Rå DMA → filtrerade mätvärden (**anropas i Loop_Measure**) |
| `bool Adaptive_ADC_CheckDMAComplete` | ISR-flagga (clear-on-read) |
| `bool Adaptive_ADC_GetMeasurement` | Senaste (kan rensa ready) |
| `bool Adaptive_ADC_GetAveraged` | Medel/IIR för display/control |
| `bool Adaptive_ADC_IsReady` | Ny data? |
| `bool Adaptive_ADC_Calibrate(..., known_vin, known_vout, known_current)` | Gain/offset |
| `uint16_t Adaptive_ADC_GetRaw(adc, channel)` | Rå kanal |

**Skalning:** `ADC_VIN_DIVIDER_RATIO`, `ADC_VOUT_DIVIDER_RATIO`, `ADC_CURRENT_V_PER_AMP` i `config.h`.

---

## 4. UART

**Header:** `hal_uart.h`

| Funktion | Beskrivning |
|----------|-------------|
| `bool Adaptive_UART_Init` | USART2 115200 |
| `bool Adaptive_UART_SendString` / `Printf` | TX (kan blockera — använd utanför ISR) |
| `bool Adaptive_UART_SendString_NonBlocking` | ISR-vänligare TX |
| `void Adaptive_UART_ProcessRX` | Anropas från UART RX IRQ |
| `bool Adaptive_UART_IsCmdReady` | Hel rad mottagen |
| `uint16_t Adaptive_UART_GetCommand(buf, max)` | Hämta rad |
| `void Adaptive_UART_ClearCommand` | Rensa |

---

## 5. Watchdog

**Header:** `hal_watchdog.h`

| Funktion | Beskrivning |
|----------|-------------|
| `bool Adaptive_WDG_Init(uint8_t timeout_level)` | IWDG |
| `void Adaptive_WDG_Refresh` | Kick |
| `bool Adaptive_WDG_WasReset` | Föregående reset var IWDG? |
| `uint32_t Adaptive_WDG_GetTimeout` | ms |

`WDG_TIMEOUT_MS` / `WDG_MS_TO_LEVEL` i `config.h`.

---

## 6. PID

**Header:** deklarationer i `config.h`, implementation `pid_controller.c`

```c
void  PID_Init(PID_Controller_t* pid, float Kp, float Ki, float Kd,
               float output_min, float output_max);
float PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt);
void  PID_Reset(PID_Controller_t* pid);
void  PID_SetGains(...);
void  PID_SetSetpointWeight(pid, float b);   // 0..1
void  PID_SetDerivativeFilter(pid, float a); // low-pass på d/dt(meas)
```

State (inkl. `d_filtered`) ligger **i struct** — reentrant mellan instanser.

Användning i bring-up: Vout-reglering inuti `Loop_Control`.

---

## 7. Parameterberäkning

**Header:** `param_calc.h`

```c
typedef struct {
    float avg_vin, avg_vout, avg_current;
    bool  averages_valid;
    float inductance_mH, capacitance_uF, esr_mOhm;
    float ripple_current, ripple_voltage, switching_freq;
    bool  dcm_detected;
    bool  valid;              // L/C/ESR OK (kräver switch-ripple-feature)
    uint32_t calc_time_ms;
} CalculatedParams_t;
```

| Funktion | Bring-up-beteende |
|----------|-------------------|
| `ParamCalc_Init` / `AddSample` / `ResetBuffer` | Alltid |
| `ParamCalc_CalculateAll` | Alltid fyller **medelvärden**; L/C/ESR bara om `FEATURE_SWITCH_RIPPLE_ESTIMATION` |
| `ParamCalc_CalculateL/C/ESR`, ripple helpers | Experimentella formler |

---

## 8. Efficiency (valfri)

**Header:** `efficiency_calc.h`

| Funktion | Notering |
|----------|----------|
| `EfficiencyCalc_Init` / `FromMeasurements` / `FromModel` | Initieras i `Tasks_Init` |
| `EfficiencyCalc_GetFiltered` | MA-filter |

Används i control-loopen **endast** om `FEATURE_EFFICIENCY_CONTROL=1`.  
`FromMeasurements` skattar Iin från antagen η om bara en strömkanal finns — **inte** giltig closed-loop-feedback i bring-up.

---

## 9. CLI

**Header:** `cli_commands.h`

| Funktion | Beskrivning |
|----------|-------------|
| `bool CLI_Init(void)` | Init auth-modul m.m. |
| `bool CLI_ProcessCommand(uart, cmd)` | Parsa + kör (anropas från Loop_CLI) |
| `uint16_t CLI_GetHelp(buf, size)` | Hjälptext |
| `bool CLI_IsAuthenticated` / `CLI_CommandRequiresAuth` | Auth-gate |

**Inbyggda handlers (urval):**  
`cmd_status`, `cmd_config`, `cmd_monitor`, `cmd_pwm`, `cmd_calibrate`, `cmd_errors`, `cmd_help`,  
diagnostik: `cmd_faults`, `cmd_safety`, `cmd_recovery`, …  
auth: `cmd_login`, `cmd_logout`, `cmd_passwd`, `cmd_authstatus` (meningsfulla när auth på).

### CLI-protokoll (text)

```text
config features
config vout <volts>
pwm start | stop | clear | <duty_percent>
status [adc|pwm|params]
monitor <seconds>
```

---

## 10. Auth (option)

**Header:** `cli_auth.h`  
**Default:** `FEATURE_CLI_AUTH=0` → `CLI_Auth_IsAuthenticated()` returnerar true alltid.

| Funktion | Roll |
|----------|------|
| `CLI_Auth_Init` / `IsEnabled` / `Login` / `Logout` | Session |
| `CLI_Auth_SetPassword` / `IsPasswordSet` | Credentials |
| `CLI_Auth_IsSetupConfirmationRequired` | SEC-031 (om setup confirm på) |

---

## 11. Fel & temp

**error_handler.h**

| Funktion | Roll |
|----------|------|
| `Error_Init` / `Error_Report` / `Error_Critical` | Logg / critical path |
| `Error_GetLog` / `Error_IsFault` / `Error_ClearFault` | Status |

**temperature_monitor.h**

| Funktion | Roll |
|----------|------|
| `TempMonitor_Init` / `Update` | Tillstånd |
| `TempMonitor_IsSafe` / `ShutdownRequired` / `GetAllowedPower` | Derating/stop |

**current_protection.h** — trösklar/enable (kan användas utöver Loop_Safety).

**fault_history.h** / **enhanced_safety.h** — utökad historik och state machine; se källkod för full yta. Bring-up förlitar sig primärt på `Loop_Safety` + latch-API i `Tasks_*`.

---

## 12. Flash logger

**flash_logger.h** — cirkulär flash-logg.  
HMAC-varianter (`flash_logger_hmac.h`) styrs av `FEATURE_FLASH_LOGGER_HMAC` (default av).

---

## 13. Feature-flaggor (compile-time)

**Header:** `config/features.h` (inkluderas via `config.h`)

| Makro | Default | Effekt |
|-------|---------|--------|
| `FEATURE_VOUT_CONTROL` | 1 | Vout-PID i Loop_Control |
| `FEATURE_EFFICIENCY_CONTROL` | 0 | η-duty |
| `FEATURE_SWITCH_RIPPLE_ESTIMATION` | 0 | L/C/ESR i ParamCalc |
| `FEATURE_CLI_AUTH` | 0 | Lösenordskrav |
| `FEATURE_SOFT_FAULT_RECOVERY` | 1 | Auto-clear latch |
| `FEATURE_SECURITY_PROFILE` | 0 | Master för flera sec-flaggor |

Överskriv med `-DFEATURE_…=1` i Makefile vid behov.

---

## 14. Globala handles (main)

Definierade i `App/Src/main.c`, used av CLI/tasks:

```c
Adaptive_PWM_t pwm_handle;
Adaptive_ADC_t adc_handle;
Adaptive_UART_t uart_handle;
TaskManager_t task_manager;
// + error_manager, temp_monitor, safety_manager, waveform_buffer, calc_params
float current_duty_cycle;
```

---

## 15. Relaterat

- Design: [design.md](design.md)  
- CLI-smoke på hårdvara: [HIL_CHECKLIST.md](HIL_CHECKLIST.md)  
- Mål: [mål.md](mål.md)
