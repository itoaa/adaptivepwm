# HIL Checklist — AdaptivePWM

**Goal:** Prove the firmware works on real hardware with a power stage / dummy load before calling anything “release”.  
**Profile under test:** default bring-up (`FEATURE_SECURITY_PROFILE=0`).

Fill one row per board/session. Status: **PASS / FAIL / SKIP / N/A**.

| Field | Value |
|-------|--------|
| Date | |
| Board | NUCLEO-F401RE + … |
| Firmware version / git SHA | |
| Input voltage | |
| Load | |
| Tester | |

---

## 0. Setup

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 0.1 | Build & flash | `pio run -e nucleo_f401re -t upload` | Completes without error | | |
| 0.2 | Serial banner | 115200 baud | Version, superloop, Auth DISABLED, Control: Vout PID | | |
| 0.3 | CLI help | `help` | Lists commands; no auth lock | | |
| 0.4 | Features | `config features` | `VOUT_CONTROL=1`, `CLI_AUTH=0`, `SWITCH_RIPPLE_EST=0` | | |

## 1. PWM / open loop

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 1.1 | Start PWM | `pwm start` | No fault; status shows Running | | |
| 1.2 | Set duty | `pwm duty 0.25` (or project syntax) | Duty ≈ 25%; scope if available | | |
| 1.3 | Soft limits | Try duty &lt;5% and &gt;95% | Clamped to soft/hard limits | | |
| 1.4 | Stop | `pwm stop` | Outputs safe / stopped | | |
| 1.5 | Emergency | BKIN / fault path if wired | PWM off immediately | | |

## 2. Measurement

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 2.1 | ADC read | `status adc` | Vin/Vout/I/T plausible vs multimeter | | |
| 2.2 | Monitor CSV | `monitor 5` | 5 lines/sec; columns fill with numbers | | |
| 2.3 | Averages | `status params` | Averages present; L/C/ESR reported disabled | | |
| 2.4 | Calibration | known Vin/Vout | After calibrate, error within target (document %) | | |

## 3. Control (efficiency path)

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 3.1 | Closed loop idle | PWM running, light load | Duty stable (no hard oscillation) | | |
| 3.2 | Load step | Step load mid-range | Recovers without trip; log with `monitor` | | |
| 3.3 | Efficiency trend | Compare Pin/Pout estimate | Directionally sensible (not &gt;100% for long) | | |

## 4. Safety limits (keep enabled)

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 4.1 | Overcurrent | Controlled overcurrent or inject | PWM emergency stop / safe state | | |
| 4.2 | Overtemp | Heat sensor or inject | Warning then shutdown path as designed | | |
| 4.3 | Undervoltage | Lower Vin | Safe stop / fault, no hang | | |
| 4.4 | Watchdog | Hang injection if available | System resets within timeout | | |

## 5. Robustness

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 5.1 | Cold start | Power cycle 5× | Always reaches CLI prompt | | |
| 5.2 | Long run | ≥30 min light load | No reset, no thermal runaway | | |
| 5.3 | UART spam | Fast typing / long lines | No crash; rejects garbage | | |

## 6. Security (SKIP on default profile)

| # | Test | Method | Pass criteria | Status | Notes |
|---|------|--------|---------------|--------|-------|
| 6.1 | Auth gate | Build with `-DFEATURE_CLI_AUTH=1` | Login required for `pwm` | | SKIP if profile=0 |
| 6.2 | Signed boot | Bootloader + signed image | Unsigned rejected | | SKIP until recovery done |

---

## Result summary

| Area | Result (PASS if all critical rows PASS) |
|------|----------------------------------------|
| Bring-up (0) | |
| PWM (1) | |
| Measurement (2) | |
| Control (3) | |
| Safety (4) | |
| **Overall HIL gate** | **☐ PASS / ☐ FAIL** |

**Release rule:** Do not tag “release” or mark MATURITY **HIL** green until **Overall HIL gate = PASS** for the intended hardware.

### Serial capture tip

```text
monitor 30
```

Save UART log next to this checklist (CSV from `monitor`).
