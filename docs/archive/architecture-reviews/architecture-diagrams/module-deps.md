# Module Dependency Diagrams

## Overview

This document describes the module dependencies in the AdaptivePWM system.

## High-Level Architecture

```mermaid
graph TD
    subgraph Application
        A[main.c] --> B[Enhanced Safety]
        A --> C[FreeRTOS Tasks]
        A --> D[HAL Layer]
        A --> E[Parameter Calc]
        A --> F[CLI System]
    end
    
    subgraph Safety_System
        B --> B1[Fault History]
        B --> B2[Watchdog]
        B --> B3[State Machine]
    end
    
    subgraph Task_System
        C --> C1[Measurement Task]
        C --> C2[Control Task]
        C --> C3[Safety Task]
        C --> C4[CLI Task]
    end
    
    subgraph HAL_Layer
        D --> D1[ADC]
        D --> D2[PWM]
        D --> D3[UART]
    end
    
    subgraph Calculation
        E --> E1[RMS Calculation]
        E --> E2[Parameter Estimation]
        E --> E3[Efficiency Calc]
    end
    
    subgraph CLI_System
        F --> F1[Auth]
        F --> F2[Commands]
    end
    
    C1 --> D1
    C2 --> D2
    C2 --> E
    C3 --> B
    C4 --> F
    F --> D3
```

## Core Module Dependencies

### Main Application Flow

```mermaid
graph LR
    Main[main.c] --> Init[System Init]
    Init --> Safety[EnhancedSafety_Init]
    Init --> FaultHist[FaultHistory_Init]
    Init --> TempMon[TempMonitor_Init]
    Init --> ParamCalc[ParamCalc_Init]
    Init --> PWM[Adaptive_PWM_Init]
    Init --> ADC[Adaptive_ADC_Init]
    Init --> UART[Adaptive_UART_Init]
    Init --> Tasks[Tasks_Init]
    
    Tasks --> Scheduler[vTaskStartScheduler]
```

### Task Dependencies

```mermaid
graph TD
    subgraph Tasks
        TM[Task_Measurement] --> ADC[ADC Read]
        TM --> PB[ParamCalc Buffer]
        
        TC[Task_Control] --> PC[ParamCalc Calculate]
        TC --> EC[EfficiencyCalc]
        TC --> PWM[PWM Update]
        
        TS[Task_Safety] --> ADC2[ADC Check]
        TS --> ES[Emergency Stop]
        
        TCL[Task_CLI] --> CMD[Process Commands]
    end
    
    TM --> TC
    TM -.->|params_ready_sem| TC
    TC -.->|duty_queue| PWM
```

## HAL Layer Dependencies

```mermaid
graph LR
    subgraph Hardware_Abstraction
        HAL_ADC --> STM32_ADC[STM32 ADC DMA]
        HAL_PWM --> STM32_TIM[STM32 TIM1]
        HAL_UART --> STM32_UART[STM32 USART2]
        HAL_WDG --> STM32_IWDG[STM32 Independent WDG]
    end
    
    subgraph Middleware
        ParamCalc --> HAL_ADC
        PID_Controller --> HAL_PWM
        CLI_Commands --> HAL_UART
        Enhanced_Safety --> HAL_WDG
    end
```

## Safety System Dependencies

```mermaid
graph TD
    subgraph Enhanced_Safety_System
        ES[EnhancedSafety_Process] --> SM[State Machine]
        ES --> RM[Recovery Manager]
        ES --> WD[Watchdog Monitor]
        ES --> DM[Diagnostic Mode]
        
        SM --> States[State Transitions]
        RM --> Actions[Recovery Actions]
        WD --> Health[Module Health]
    end
    
    subgraph Fault_Handling
        FH[FaultHistory_Log] --> Flash[Flash Storage]
        FH --> CRC[CRC Validation]
    end
    
    ES --> FH
```

## Data Flow

```mermaid
graph LR
    ADC[ADC Samples] --> Buffer[Waveform Buffer]
    Buffer --> RMS[RMS Calculation]
    RMS --> Params[Parameters]
    Params --> Control[Control Loop]
    Control --> PWM[PWM Output]
    Control --> Eff[Efficiency]
```

## Build Dependencies

```mermaid
graph TD
    subgraph Source_Files
        main.c
        enhanced_safety.c
        freertos_tasks.c
        param_calc.c
        efficiency_calc.c
        pid_controller.c
        cli_auth.c
        cli_commands.c
        fault_history.c
        flash_logger.c
    end
    
    subgraph HAL_Files
        hal_adc.c
        hal_pwm.c
        hal_uart.c
        hal_watchdog.c
    end
    
    subgraph Libraries
        FreeRTOS
        STM32_HAL
        Math_Lib
    end
    
    main.c --> enhanced_safety.c
    main.c --> freertos_tasks.c
    main.c --> HAL_Files
    freertos_tasks.c --> param_calc.c
    freertos_tasks.c --> efficiency_calc.c
    freertos_tasks.c --> pid_controller.c
    freertos_tasks.c --> HAL_Files
    enhanced_safety.c --> fault_history.c
    fault_history.c --> flash_logger.c
    cli_commands.c --> cli_auth.c
    cli_commands.c --> HAL_Files
```
