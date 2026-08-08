# AdaptivePWM — mognadsmatris

**Senast uppdaterad:** 2026-08-08  
**Default-profil:** bring-up (`config/features.h`, `FEATURE_SECURITY_PROFILE=0`)  
**Bygge:** Makefile / CubeIDE → `build/cubeide/AdaptivePWM.elf`

Detta dokument är **release-sanning**. Historiska “100 % klart”-dokument ligger i `docs/archive/`.

## Statusnivåer

| Nivå | Betydelse |
|------|-----------|
| Design | Spec/docs |
| Kod | Finns i `App/` / `Core/` |
| Unit | Host-/enhetstest |
| HIL | Godkänd checklista på hårdvara + last |
| Field | Verifierad i applikation |

## Kärnfunktion (projektets syfte)

| Feature | Design | Kod | Unit | HIL | Field | Kommentar |
|---------|:------:|:---:|:----:|:---:|:-----:|-----------|
| PWM TIM1 + dead-time | ✓ | ✓ | delvis | ☐ | ☐ | |
| ADC Vin/Vout/I/Temp + filter | ✓ | ✓ | delvis | ☐ | ☐ | Skala i `config.h` |
| Bare-metal superloop | ✓ | ✓ | ☐ | ☐ | ☐ | measure/control/safety/CLI |
| **Vout-PID** | ✓ | ✓ | ☐ | ☐ | ☐ | Primär reglering |
| Soft safety latch | ✓ | ✓ | ☐ | ☐ | ☐ | `pwm clear` / cooldown |
| UART CLI | ✓ | ✓ | ☐ | ☐ | ☐ | Ej i IRQ |
| Effektivitets-duty-loop | ✓ | ✓ | delvis | ☐ | ☐ | **Av** default |
| L/C/ESR switch-ripple | ✓ | ✓ | delvis | ☐ | ☐ | **Av** default |

## Security (valfritt, senare)

| Feature | Design | Kod | HIL | Kommentar |
|---------|:------:|:---:|:---:|-----------|
| CLI-auth | ✓ | ✓ | ☐ | `FEATURE_CLI_AUTH=0` |
| Flash HMAC | ✓ | delvis | ☐ | av default |
| Secure bootloader | ✓ | delvis | ☐ | recovery/flash TODO |
| HW RNG | n/a | — | — | F401 saknar RNG |

## Nästa steg (mot syftet)

1. Kör [HIL_CHECKLIST.md](docs/HIL_CHECKLIST.md) på Nucleo + effektsteg.  
2. Kalibrera ADC-skalning för din hårdvara.  
3. Trimma Vout-PID / setpoint.  
4. Först därefter: ripple-estimering eller security-profil.

## Aktivera security senare

```c
// config/features.h
#define FEATURE_SECURITY_PROFILE  1
```

Endast när recovery, nyckelhantering och auth är kompletta end-to-end.
