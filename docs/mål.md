# Mål

AdaptivePWM ska ge **realtids PWM-reglering** av DC/DC (buck/boost m.fl.) med mätning och skydd.

## Kärnmål (prioriterade)

1. **Styrbar PWM** — 20 kHz-klass, dead-time, duty-gränser, mjuk ramp.  
2. **Mätning** — Vin, Vout, ström, temperatur med skalning och filtrering.  
3. **Closed-loop** — i bring-up: **Vout-PID** mot setpoint (mätbar feedback).  
4. **Safe operating area** — överström/överspänning/temp; mjuk fault + återstart via CLI.  
5. **Observability** — UART-CLI, `monitor`-CSV för HIL och lab.  

## Sekundära / senare mål

| Mål | Kommentar |
|-----|-----------|
| Realtids L/C/ESR från ripple | Kräver sampling ≫ f_sw; av som default |
| Effektivitetsoptimering som primär loop | Kräver giltig Pin/Pout; av som default |
| Secure boot, CLI-auth, logg-HMAC | Option via `FEATURE_SECURITY_PROFILE` |
| SIL 2 / formell cert | Processmål, inte “klart” via kodkommentarer |

## Icke-mål (medvetet)

- Inte SIL 3/4 eller medicin/aerospace utan separat process.  
- Inte “produktionslåst” flash (RDP) i default bring-up.  
- Inte cloud/app-backend.
