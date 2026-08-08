# Configuration

## Primary: `features.h`

**This is the main switchboard for optional features.**

Default profile is **bring-up / lab**: security hardening off, switch-ripple L/C/ESR off, efficiency control on.

| Flag | Default | Effect |
|------|---------|--------|
| `FEATURE_SECURITY_PROFILE` | 0 | Master: sets several security flags when 1 |
| `FEATURE_CLI_AUTH` | 0 | UART password login |
| `FEATURE_SETUP_CONFIRM` | 0 | Physical button for first password |
| `FEATURE_FLASH_LOGGER_HMAC` | 0 | HMAC on flash log entries |
| `FEATURE_SECURE_BOOT` | 0 | Awareness flag (bootloader is separate) |
| `FEATURE_HARDWARE_RNG` | 0 | F401 has no HW RNG |
| `FEATURE_EFFICIENCY_CONTROL` | 1 | Duty adjust from measured efficiency |
| `FEATURE_SWITCH_RIPPLE_ESTIMATION` | 0 | Experimental L/C/ESR from ripple |

### Enable security later

Edit `config/features.h`:

```c
#define FEATURE_SECURITY_PROFILE  1
```

Or pass build flags (PlatformIO `build_flags`):

```
-DFEATURE_CLI_AUTH=1
```

See `MATURITY.md` before treating security as “done”.

## Board-specific headers

Optional board files may be added here later. Core runtime config remains `src/config.h` (includes `features.h`).

Include path: PlatformIO adds `-Iconfig` so `#include "features.h"` works from firmware sources.
