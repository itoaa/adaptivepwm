# Tests

Host-/enhetstester (C). Firmware byggs med root-`Makefile`, inte härifrån.

```bash
# Exempel (kräver gcc + ev. mocks)
# make -C tests   # om Unity och mocks är på plats
```

Många tester skrevs mot äldre HAL-mock-upplägg. **HIL-checklistan** är den viktiga valideringen mot projektets syfte (reglering på hårdvara).
