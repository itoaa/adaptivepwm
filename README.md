# AdaptivePWM

[![CI](https://github.com/itoaa/adaptivepwm/actions/workflows/ci.yml/badge.svg)](https://github.com/itoaa/adaptivepwm/actions/workflows/ci.yml)

**Realtids PWM-styrning för buck/boost-omvandlare** (och liknande effektsteg) på **STM32F401RE** (NUCLEO-F401RE).

## Vad projektet försöker åstadkomma

Målet är en **inbyggd regulator** som:

1. **Mäter** spänning, ström och temperatur (ADC + filter).
2. **Styr** PWM (TIM1, dead-time, ramping) så att utgången hålls i ett säkert och effektivt arbetsläge.
3. **Reglerar utspänning** — ideal duty från Vin och börvärde (feedforward), PID bara som trim.  
4. **Håller safe operating area** — överström i hårdvara (TIM1 BKIN + ADC-watchdog), överspänning/temp i mjukvara, mjuk fault-latch.
5. **Ger labb-/fältverktyg** — UART-CLI (`status`, `pwm`, `monitor`, `config vout`, …).
6. **Lämnar utrymme för senare hårdning** — auth, HMAC, secure boot som *valfria* flaggor (av som default).

Historiskt nämns också realtids-L/C/ESR och ren “effektivitetsjakt”; det är **medvetet nedprioriterat/avstängt** tills sampling och HIL bevisar det (se `config/features.h` och `MATURITY.md`).

| Mål (kärna) | Status i bring-up |
|-------------|-------------------|
| PWM + mätning + Vout-feedforward | Kod + superloop; HIL på last kvarstår |
| Överström via BKIN/ADC-watchdog, övriga gränser i loopen | Kod; validera på hårdvara |
| Effektivitet / L-C från ripple | Av default (fysik/sampling) |
| Security (login, secure boot, …) | Option, av default |

## Snabbstart (bygga)

Primär väg: **Makefile / STM32CubeIDE** (inte PlatformIO).

```bash
# Ubuntu: sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi make
make -j"$(nproc)"
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
| `FEATURE_VOUT_CONTROL` | 1 | Vout-feedforward + PID-trim |
| `FEATURE_EFFICIENCY_CONTROL` | 0 | Experimentell η→duty (av) |
| `FEATURE_SWITCH_RIPPLE_ESTIMATION` | 0 | L/C/ESR från ripple (av) |
| `FEATURE_SECURITY_PROFILE` | 0 | Master för auth/HMAC m.m. |
| `FEATURE_CLI_AUTH` | 0 | UART-lösenord |
| `FEATURE_SOFT_FAULT_RECOVERY` | 1 | Mjuk recovery efter fault |

ADC-skalning, topologi och strömgräns: `App/Inc/config.h`.

| Konstant | Default | Betydelse |
|----------|---------|-----------|
| `CONVERTER_TOPOLOGY` | 0 (buck) | 0 buck `Vout/Vin`, 1 boost `1-Vin/Vout`, 2 buck-boost |
| `VOUT_SETPOINT_DEFAULT_V` | 1,5 V | Labbbörvärde i pin-domän |
| `ADC_*_DIVIDER_RATIO` | 1,0 | Pin-volt → riktig spänning |
| `ADC_CURRENT_V_PER_AMP` | 1,0 | Volt på strömpinnen per ampere |
| `CURRENT_MAX_A` | 10 A | Watchdog-tröskel. Vid 1 V/A ryms inte 10 A i 3,3 V — sätt skalan innan strömtest |

**Nucleo-pinnar:** PWM `PA8`/`PA9`, BKIN `PA6` (aktiv låg, pull-up), Vin `PA0`, Vout `PA1`. USART2 (ST-Link, 115200) tar `PA2`/`PA3` **efter** ADC-init, så ström och temperatur på de pinnarna mäts inte medan CLI:t är igång. Se [docs/HIL_CHECKLIST.md](docs/HIL_CHECKLIST.md).

Värd-CLI:t i `cli/` öppnar inte serieporten. Labbet styrs av firmware-CLI:t ovan.

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
