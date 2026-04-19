# Data Flow Documentation

## Overview

This document describes the data flow through the AdaptivePWM system.

## System Data Flow

```mermaid
graph LR
    subgraph Input
        ADC_DMA[ADC DMA<br/>1kHz Sampling] --> Raw[Raw Samples<br/>Vin/Vout/Current]
    end
    
    subgraph Processing
        Raw --> RMS[RMS Calculation<br/>32-sample window]
        RMS --> Params[Parameter Estimation<br/>L, C, ESR]
        Params --> Eff[Efficiency Calculation<br/>Measurement-based]
    end
    
    subgraph Control
        Eff --> PID[PID Controller<br/>Target: 95% efficiency]
        PID --> PWM[PWM Update<br/>10Hz control loop]
    end
    
    subgraph Output
        PWM --> HW[Hardware PWM<br/>20kHz switching]
    end
    
    subgraph Safety_Monitor
        Raw --> Safety[Safety Checks<br/>100Hz monitoring]
        Safety --> Emergency[Emergency Stop<br/>if fault detected]
    end
```

## ADC Data Flow

```mermaid
sequenceDiagram
    participant ADC as ADC Hardware
    participant DMA as DMA Controller
    participant HAL as HAL_ADC
    participant Task as Task_Measurement
    participant Buffer as WaveformBuffer
    
    loop 1kHz Sampling
        ADC->>DMA: Sample complete
        DMA->>HAL: Transfer complete
        HAL->>Task: Signal ready
        Task->>Buffer: AddSample()
        Task->>Task: Signal params_ready_sem
    end
```

## Control Loop Data Flow

```mermaid
sequenceDiagram
    participant Control as Task_Control
    participant Sem as params_ready_sem
    participant Param as ParamCalc
    participant Eff as EfficiencyCalc
    participant PWM as HAL_PWM
    
    loop 100Hz Control Loop
        Control->>Sem: Take semaphore (10ms timeout)
        Sem-->>Control: Semaphore acquired
        Control->>Param: Get measurements
        Param-->>Control: ADC_Measurement_t
        Control->>Eff: Calculate efficiency
        Eff-->>Control: efficiency value
        Control->>Control: PID calculation
        Control->>PWM: Set duty cycle
    end
```

## Safety System Data Flow

```mermaid
sequenceDiagram
    participant Safety as Task_Safety
    participant ADC as HAL_ADC
    participant Enhanced as EnhancedSafety
    participant Fault as FaultHistory
    participant Flash as Flash Logger
    
    loop 100Hz Safety Loop
        Safety->>ADC: Get averaged measurements
        ADC-->>Safety: ADC_Measurement_t
        Safety->>Safety: Check thresholds
        alt Fault detected
            Safety->>Enhanced: ReportFault()
            Enhanced->>Fault: Log fault
            Fault->>Flash: Write to flash
            Enhanced->>Enhanced: Attempt recovery
        end
    end
```

## CLI Data Flow

```mermaid
sequenceDiagram
    participant UART as HAL_UART
    participant CLI as CLI_ProcessCommand
    participant Auth as CLI_Auth
    participant Cmd as Command Handler
    participant System as Target System
    
    UART->>CLI: Command received
    CLI->>Auth: Check authentication
    alt Authenticated or public command
        Auth-->>CLI: Access granted
        CLI->>Cmd: Execute command
        Cmd->>System: Read/write parameters
        System-->>Cmd: Result
        Cmd-->>CLI: Response string
        CLI-->>UART: Send response
    else Not authenticated
        Auth-->>CLI: Access denied
        CLI-->>UART: Auth required
    end
```

## Fault History Data Flow

```mermaid
sequenceDiagram
    participant Caller as Fault Reporter
    participant FH as FaultHistory_Log
    participant Ring as Ring Buffer
    participant HMAC as HMAC Calculation
    participant Flash as Flash Storage
    
    Caller->>FH: Log fault
    FH->>Ring: Add entry
    FH->>HMAC: Calculate HMAC
    FH->>Flash: Write entry + HMAC
    Flash-->>FH: Confirm write
    FH-->>Caller: Return status
```

## Efficiency Calculation Data Flow

```mermaid
graph TD
    subgraph Input_Data
        ADC[ADC Measurements<br/>Vin, Vout, Iin, Iout]
        Duty[Duty Cycle]
        Fsw[Switching Frequency]
    end
    
    subgraph Calculation_Modes
        M1[MEASUREMENT Mode<br/>Primary method]
        M2[MODEL Mode<br/>Physics-based fallback]
        M3[HYBRID Mode<br/>Combined approach]
    end
    
    subgraph Output
        Eff[Efficiency %]
        Pin[Input Power]
        Pout[Output Power]
        Valid[Validity Flag]
    end
    
    ADC --> M1
    ADC --> M2
    Duty --> M2
    Fsw --> M2
    
    M1 --> Eff
    M1 --> Pin
    M1 --> Pout
    M1 --> Valid
    
    M2 --> Eff
    M2 --> Pin
    M2 --> Pout
    M2 --> Valid
```

## Parameter Estimation Data Flow

```mermaid
graph TD
    subgraph Raw_Samples
        Vin[Vin Samples]
        Vout[Vout Samples]
        I[Current Samples]
    end
    
    subgraph Calculations
        RMS_I[RMS Current<br/>32 samples]
        RMS_V[RMS Voltage<br/>32 samples]
        Freq[Frequency Detection<br/>Zero crossings]
        DCM[DCM Detection<br/>Current analysis]
    end
    
    subgraph Parameters
        L[Inductance L]
        C[Capacitance C]
        ESR[ESR]
    end
    
    I --> RMS_I
    Vout --> RMS_V
    I --> Freq
    I --> DCM
    
    RMS_I --> L
    RMS_V --> L
    Freq --> L
    DCM --> L
    
    RMS_I --> C
    RMS_V --> C
    Freq --> C
    DCM --> C
    
    RMS_I --> ESR
    RMS_V --> ESR
    DCM --> ESR
```

## Recovery Data Flow

```mermaid
graph TD
    Fault[Fault Detected] --> Severity{Severity?}
    
    Severity -->|INFO| LogOnly[Log only]
    Severity -->|WARNING| Degrade[Graceful Degradation]
    Severity -->|ERROR| Recovery[Attempt Recovery]
    Severity -->|CRITICAL/FATAL| Emergency[Emergency Stop]
    
    Recovery --> Mapping[Recovery Mapping]
    Mapping --> Action{Action Type}
    
    Action -->|RETRY| Retry[Retry Operation]
    Action -->|RESET| Reset[Module Reset]
    Action -->|DEGRADE| DegradeAction[Enter Degraded Mode]
    Action -->|STOP| Stop[Safe Stop]
    Action -->|RESTART| Restart[System Restart]
    
    Retry --> Success{Success?}
    Reset --> Success
    DegradeAction --> Success
    
    Success -->|Yes| Normal[Return to Normal]
    Success -->|No| FailState[Enter Failure State]
    
    LogOnly --> Flash[Flash Log]
    Degrade --> Flash
    Normal --> Flash
    FailState --> Flash
    Emergency --> Flash
```
