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
| 0.2 | Seriell 115200 | Banner, superloop, Auth DISABLED, Vout PID | |
| 0.3 | `help` / `config features` | `VOUT_CONTROL=1`, `CLI_AUTH=0` | |

## 1. PWM (öppen slinga)

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 1.1 | `pwm start` | Running | |
| 1.2 | `pwm 25` (duty %) | Duty ≈ 25 % | |
| 1.3 | `pwm stop` | Stoppad | |

## 2. Mätning

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 2.1 | `status adc` | Siffror rimliga vs multimeter | |
| 2.2 | `monitor 5` | CSV 5 rader | |
| 2.3 | Skalning | Justera divider/V_per_A vid behov | |

## 3. Vout-reglering (kärnmål)

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 3.1 | `config vout <mål>` + `pwm start` | Vout närmar sig SP | |
| 3.2 | Laststeg | Stabiliserar utan trip | |
| 3.3 | Långkörning ≥15 min | Inget reset/runaway | |

## 4. Safety

| # | Test | Kriterie | Status |
|---|------|----------|--------|
| 4.1 | Överström (kontrollerat) | PWM stop, latch | |
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
