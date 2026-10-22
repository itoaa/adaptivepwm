# HIL-checklista — AdaptivePWM

**Syfte:** Bevisa att mätning, PWM, Vout-reglering och safety fungerar på **riktig hårdvara** (projektets kärnmål).  
**Profil:** bring-up (`FEATURE_SECURITY_PROFILE=0`, Vout-control på).

| Fält | Värde |
|------|--------|
| Datum | |
| Kort / last | NUCLEO-F401RE + … |
| Firmware / git SHA | |
| Vin / last | |
| Testare | |

## 0. Bring-up

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 0.1 | `make` + flash | Lyckas | |
| 0.2 | Seriell 115200 på ST-Link | Banner, superloop, Auth DISABLED, `Vout feedforward + PID trim` | |
| 0.3 | `help` / `config features` | `VOUT_CONTROL=1`, `CLI_AUTH=0` | |
| 0.4 | Pinnar | PA8/PA9 PWM, PA6 BKIN hög (pull-up), PA0/PA1 analog. PA2/PA3 är UART — inte shunt/NTC | |

## 1. PWM (öppen slinga)

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 1.1 | `pwm start` | Running | |
| 1.2 | `pwm 25` (duty %) | Duty ≈ 25 % | |
| 1.3 | `pwm stop` | Stoppad | |

## 2. Mätning

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 2.1 | `status adc` | Vin/Vout rimliga mot multimeter. Ström/temp på PA2/PA3 är inte giltiga medan UART äger pinnarna | |
| 2.2 | `monitor 5` | CSV 5 rader | |
| 2.3 | Skalning | Justera divider/V_per_A vid behov | |

## 3. Vout-reglering (kärnmål)

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 3.1 | `config vout <mål>` + `pwm start` | Duty nära Vout/Vin (buck) och Vout närmar sig SP | |
| 3.2 | Laststeg | Stabiliserar utan trip | |
| 3.3 | Långkörning ≥15 min | Inget reset/runaway | |

## 4. Safety

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 4.1 | Dra PA6 till GND (BKIN) | PWM slocknar i hårdvara, latch, `pwm clear` krävs | |
| 4.1b | ADC-watchdog | Endast när strömkanalen är analog och `CURRENT_MAX_A` ryms i pinnen. Inte PA2 medan USART2 äger den | |
| 4.2 | `pwm clear` + start | Kan återstartas | |
| 4.3 | Övertemp (om möjligt) | Safe path | |

## Resultat

| Område | PASS/FAIL |
|--------|-----------|
| Bring-up | |
| PWM | |
| Mätning | |
| **Vout-loop** | |
| Safety | |
| **HIL totalt** | |

Release: markera HIL i `MATURITY.md` endast om **HIL totalt = PASS**.
