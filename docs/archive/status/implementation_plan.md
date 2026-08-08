# AdaptivePWM Implementation Plan

## 20 Komponenter att Implementera

### Kritiska (1-5)
1. ✅ PWM HAL (TIM1) - Färdig
2. ✅ GPIO init - Inkluderat i PWM HAL
3. ✅ ADC-DMA - Färdig
4. ✅ L-beräkning - Färdig
5. ✅ C-beräkning - Färdig

### Hög prioritet (6-10)
6. ✅ ESR-beräkning - Färdig
7. ✅ FreeRTOS tasks - Färdig
8. ⏳ Watchdog IWDG - Pågår
9. ⏳ UART HAL - Pågår
10. ⏳ UART CLI integration - Ej startad

### Medel prioritet (11-17)
11. ⏳ Error handler med PWM stop - Ej startad
12. ⏳ Temperaturövervakning - Ej startad
13. ⏳ Komplett HAL-lager - Delvis
14. ⏳ Överströmsskydd - Ej startad
15. ⏳ Kalibreringsrutin - Ej startad
16. ⏳ Dataloggning till flash - Ej startad
17. ⏳ CLI-kommandon - Ej startad

### Låg prioritet (18-20)
18. ⏳ Build modes - Ej startad
19. ⏳ Doxygen - Ej startad
20. ⏳ Enhetstester - Ej startad

## Färdiga filer:
- src/hal_pwm.h, src/hal_pwm.c
- src/hal_adc.h, src/hal_adc.c
- src/param_calc.h, src/param_calc.c
- src/freertos_tasks.h, src/freertos_tasks.c
- src/hal_uart.h (påbörjad)
