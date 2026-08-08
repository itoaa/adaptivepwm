# AdaptivePWM — Maturity Matrix (honest status)

**Last updated:** 2026-08-08  
**Default profile:** bring-up / lab (`config/features.h` → `FEATURE_SECURITY_PROFILE=0`)

This document replaces optimistic “100% complete / production-ready” claims.
Status levels:

| Level | Meaning |
|-------|---------|
| **Design** | Spec / docs exist |
| **Code** | Source present |
| **Unit** | Host unit tests exist |
| **HIL** | Passed hardware-in-the-loop on power stage (see `docs/HIL_CHECKLIST.md`) |
| **Field** | Validated in real application |

---

## Core control (priority: make it work)

| Feature | Design | Code | Unit | HIL | Field | Notes |
|---------|:------:|:----:|:----:|:---:|:-----:|-------|
| PWM TIM1 + dead-time | ✓ | ✓ | partial | ☐ | ☐ | Needs real FET stage confirmation |
| ADC DMA Vin/Vout/I/Temp | ✓ | ✓ | partial | ☐ | ☐ | Scale via `ADC_*_DIVIDER` / `ADC_CURRENT_V_PER_AMP` |
| Runtime loop | ✓ | ✓ | ☐ | ☐ | ☐ | **Bare-metal superloop** by default (measure/control/safety/CLI) |
| Vout PID regulation | ✓ | ✓ | ☐ | ☐ | ☐ | **Primary path** (`FEATURE_VOUT_CONTROL=1`) |
| Efficiency-based duty | ✓ | ✓ | partial | ☐ | ☐ | OFF by default (circular η risk) |
| Soft safety latch | ✓ | ✓ | ☐ | ☐ | ☐ | No infinite hang; `pwm clear` / cooldown |
| UART CLI (not in IRQ) | ✓ | ✓ | ☐ | ☐ | ☐ | Commands in superloop; async `monitor` |
| L/C/ESR from switch ripple | ✓ | ✓ | partial | ☐ | ☐ | **OFF by default** |

## Security (priority: later — all optional)

| Feature | Design | Code | Unit | HIL | Field | Notes |
|---------|:------:|:----:|:----:|:---:|:-----:|-------|
| CLI password auth | ✓ | ✓ | partial | ☐ | ☐ | `FEATURE_CLI_AUTH` default **0** |
| Setup button confirm | ✓ | ✓ | ☐ | ☐ | ☐ | `FEATURE_SETUP_CONFIRM` default **0** |
| Flash log HMAC | ✓ | partial | partial | ☐ | ☐ | `FEATURE_FLASH_LOGGER_HMAC` default **0** |
| Secure bootloader Ed25519 | ✓ | partial | partial | ☐ | ☐ | Recovery flash/YMODEM still TODO |
| RDP / SWD lockdown scripts | ✓ | script | ☐ | ☐ | ☐ | Manual production step only |
| Hardware RNG | ✓ | n/a | ☐ | ☐ | ☐ | **STM32F401 has no RNG** |

## Deferred work (do after basic HIL is green)

1. **Security profile** — flip `FEATURE_SECURITY_PROFILE=1` only after auth + key mgmt + recovery path are complete end-to-end.
2. **Switch-ripple L/C** — requires capture rate ≫ `PWM_FREQUENCY_HZ` (or dedicated sample-and-hold); then `FEATURE_SWITCH_RIPPLE_ESTIMATION=1`.
3. **Bootloader recovery** — implement flash programming + transfer protocol (TODOs in `bootloader/secure_bootloader.c`).
4. **Private signing keys** — never store in git; see `keys/README.md`.
5. **SIL 2 / IEC claims** — process + independent V&V, not compile flags.

---

## How to enable security later

```c
// config/features.h  OR build flags:
#define FEATURE_SECURITY_PROFILE  1
// or individually:
// -DFEATURE_CLI_AUTH=1 -DFEATURE_FLASH_LOGGER_HMAC=1
```

Do not claim residual CVSS = 0 until the maturity row reaches **HIL** for that control.
