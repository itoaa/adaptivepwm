# AdaptivePWM Implementation Status

**Superseded for honesty by:** [`MATURITY.md`](MATURITY.md)

Do not use older “100% complete / produktionsklar” language. Current intent:

1. **Get control + measurement working** on hardware (default feature profile).
2. Run **HIL checklist** (`docs/HIL_CHECKLIST.md`).
3. Only then enable optional security via `config/features.h`.

## Default feature profile (bring-up)

| Area | Status |
|------|--------|
| PWM / ADC / FreeRTOS / CLI | Code present — HIL pending |
| Efficiency-based control | Enabled (`FEATURE_EFFICIENCY_CONTROL=1`) |
| L/C/ESR switch-ripple | **Disabled** (not Nyquist-safe at default rates) |
| CLI auth, HMAC, secure boot | **Disabled** (optional later) |
| Private signing keys in repo | **Removed** — see `keys/README.md` |

## Security deferred

All higher security requirements are compile-time options in `config/features.h`, default **off**.  
Enable with `FEATURE_SECURITY_PROFILE=1` or individual flags when ready.
