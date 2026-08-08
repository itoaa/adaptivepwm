# AdaptivePWM

[![CI](https://github.com/itoaa/adaptivepwm/actions/workflows/ci.yml/badge.svg)](https://github.com/itoaa/adaptivepwm/actions/workflows/ci.yml)

**Realtids PWM-styrning för buck/boost-omvandlare** (och liknande effektsteg) på **STM32F401RE** (NUCLEO-F401RE).

## Vad projektet försöker åstadkomma

Målet är en **inbyggd regulator** som:

1. **Mäter** spänning, ström och temperatur (ADC + filter).
2. **Styr** PWM (TIM1, dead-time, ramping) så att utgången hålls i ett säkert och effektivt arbetsläge.
3. **Reglerar utspänning** (Vout-PID) som primär feedback — mätbar och användbar på riktig hårdvara.
4. **Håller safe operating area** — gränser för ström/spänning/temp, mjuk fault-latch (ingen oändlig hang).
5. **Ger labb-/fältverktyg** — UART-CLI (`status`, `pwm`, `monitor`, `config vout`, …).
6. **Lämnar utrymme för senare hårdning** — auth, HMAC, secure boot som *valfria* flaggor (av som default).

Historiskt nämns också realtids-L/C/ESR och ren “effektivitetsjakt”; det är **medvetet nedprioriterat/avstängt** tills sampling och HIL bevisar det (se `config/features.h` och `MATURITY.md`).

| Mål (kärna) | Status i bring-up |
|-------------|-------------------|
| PWM + mätning + Vout-reglering | Kod + superloop; HIL på last kvarstår |
| Säkerhetsgränser + soft recovery | I loopen; validera på hårdvara |
| Effektivitet / L-C från ripple | Av default (fysik/sampling) |
| Security (login, secure boot, …) | Option, av default |

## Snabbstart (bygga)

Primär väg: **Makefile / STM32CubeIDE** (inte PlatformIO).

```bash
# Behöver arm-none-eabi-gcc + make
make -j8
# → build/cubeide/AdaptivePWM.{elf,bin,hex}
```

Öppna i CubeIDE: se **[CUBEIDE.md](CUBEIDE.md)**.

Flash (ST-Link / CubeProgrammer), sedan seriell **115200 8N1**.

## CLI (lab)

```text
help
status
status adc
config features
config vout 1.5
pwm start
monitor 10
pwm stop
pwm clear          # rensa safety-latch
```

## Feature-flaggor

Allt “tungt” styrs i **`config/features.h`** (default = bring-up):

| Flagga | Default | Betydelse |
|--------|---------|-----------|
| `FEATURE_VOUT_CONTROL` | 1 | Vout-PID (primär reglering) |
| `FEATURE_EFFICIENCY_CONTROL` | 0 | Experimentell η→duty (av) |
| `FEATURE_SWITCH_RIPPLE_ESTIMATION` | 0 | L/C/ESR från ripple (av) |
| `FEATURE_SECURITY_PROFILE` | 0 | Master för auth/HMAC m.m. |
| `FEATURE_CLI_AUTH` | 0 | UART-lösenord |
| `FEATURE_SOFT_FAULT_RECOVERY` | 1 | Mjuk recovery efter fault |

ADC-skalning (divider, V/A): `App/Inc/config.h` (`ADC_VIN_DIVIDER_RATIO`, …).

## Repostruktur

```text
App/           Applikationsfirmware (enda källträdet)
Core/          HAL-conf, MSP, syscalls, system_stm32f4xx
Drivers/       CMSIS + HAL (F401-subset)
Startup/       startup_stm32f401xe.s
config/        features.h
docs/          Aktiv dokumentation + archive/
bootloader/    Secure boot (ofullständig recovery — se MATURITY)
tests/         Host/enhetstester (källkod)
Makefile       Primärt bygge
```

## Dokumentation (läsordning)

1. **Detta README** — syfte och start  
2. **[MATURITY.md](MATURITY.md)** — vad som är klart / inte klart  
3. **[CUBEIDE.md](CUBEIDE.md)** — IDE och bygge  
4. **[docs/HIL_CHECKLIST.md](docs/HIL_CHECKLIST.md)** — verifiera på hårdvara  
5. **[docs/mål.md](docs/mål.md)** · **[docs/design.md](docs/design.md)** · **[docs/api.md](docs/api.md)** — mål, design, API  
6. **[docs/archive/](docs/archive/)** — historik (gamla status/SEC/PlatformIO) — *inte* release-sanning  

## Status & ärlighet

Använd **`MATURITY.md`** som status, inte gamla “100 % produktionsklar”-dokument (de ligger i `docs/archive/`).

CI bygger Makefile-target på varje push till `main`.

## Licens

Se [LICENSE](LICENSE).
