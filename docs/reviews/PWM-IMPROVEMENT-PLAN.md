# AdaptivePWM Improvement Plan

**Document ID:** PWM-IMPROVEMENT-PLAN  
**Version:** 1.0  
**Date:** 2026-04-16  
**Priority Framework:** CISSP/NIST Risk-Based Prioritization  
**Classification:** Internal Development Roadmap

---

## Executive Summary

This improvement plan prioritizes architectural enhancements for AdaptivePWM based on the architecture review findings. Items are categorized by **Impact** (benefit) and **Effort** (implementation cost), then prioritized by **Risk** (security/safety implications).

### Priority Matrix

```
                    LOW EFFORT          HIGH EFFORT
                ┌─────────────────┬─────────────────┐
     HIGH       │   Quick Wins    │  Major Projects │
     IMPACT     │  (Do First)     │  (Plan Carefully)│
                │                 │                 │
                │ • Split config.h│ • HAL Portability│
                │ • Add PID tests │ • Secure Boot   │
                │ • Global struct │ • Redundant ADC │
                │ • Static analysis│                │
                ├─────────────────┼─────────────────┤
     LOW        │  Fill-ins       │  Avoid          │
     IMPACT     │  (Do When Free) │  (Defer/Skip)   │
                │                 │                 │
                │ • Const correct │ • Major rewrites│
                │ • Magic numbers │ • New features  │
                │ • Comments      │ • Scope creep   │
                └─────────────────┴─────────────────┘
```

---

## Priority Categories

### 🔴 Critical Priority (Sprint 0 - Immediate)

Security/Safety blockers that must be addressed before production use.

---

### 🟠 High Priority (Sprint 1-2)

Architectural improvements that significantly improve maintainability or safety.

---

### 🟡 Medium Priority (Sprint 3-4)

Enhancements that improve code quality and developer experience.

---

### 🟢 Low Priority (Backlog)

Nice-to-have improvements for technical debt reduction.

---

## Detailed Improvement Items

### 🔴 CRITICAL-001: Refactor Global State Pollution

**Current State:**
```c
// main.c - Global externals
Adaptive_PWM_t pwm_handle;
Adaptive_ADC_t adc_handle;
Adaptive_UART_t uart_handle;
TaskManager_t task_manager;
ErrorManager_t error_manager;
// ... 8+ more
```

**Problem:**
- Breaks encapsulation
- Makes unit testing impossible
- Creates hidden dependencies
- Race condition risks

**Solution:** Implement Dependency Injection via Context Structure

```c
// system_context.h
typedef struct {
    Adaptive_PWM_t* pwm;
    Adaptive_ADC_t* adc;
    Adaptive_UART_t* uart;
    TaskManager_t* tasks;
    ErrorManager_t* errors;
    EnhancedSafetyManager_t* safety;
    TempMonitor_t* temp;
    FlashLogger_t* logger;
    // ... all handles
} SystemContext_t;

// Pass context to all modules
bool HAL_PWM_Init(Adaptive_PWM_t* pwm, const SystemContext_t* ctx);
bool Tasks_Init(TaskManager_t* tasks, SystemContext_t* ctx);
```

**Tasks:**
1. [ ] Create `system_context.h` with context structure
2. [ ] Update all HAL modules to accept context pointer
3. [ ] Update task initialization
4. [ ] Update CLI command handlers
5. [ ] Update safety system callbacks
6. [ ] Remove global externals from main.c
7. [ ] Update unit tests to use context injection

**Effort:** 16 hours  
**Impact:** High (enables testing, improves architecture)  
**Risk:** Medium (regression potential)  
**Dependencies:** None

---

### 🔴 CRITICAL-002: Break Circular Safety Dependencies

**Current Cycle:**
```
enhanced_safety.c → fault_history.c → fault_handler.c → enhanced_safety.c
```

**Solution:** Event-Driven Architecture

```c
// safety_events.h
typedef void (*SafetyEventHandler_t)(FaultEvent_t event);

// Register handlers instead of direct calls
void Safety_RegisterHandler(SafetyEventType_t type, SafetyEventHandler_t handler);
void Safety_NotifyEvent(SafetyEventType_t type, const FaultEvent_t* event);
```

**Tasks:**
1. [ ] Create event dispatcher module
2. [ ] Define event types and structures
3. [ ] Refactor fault_history to emit events
4. [ ] Refactor fault_handler to subscribe to events
5. [ ] Refactor enhanced_safety as event consumer
6. [ ] Update main.c initialization order
7. [ ] Add event queue overflow protection

**Effort:** 12 hours  
**Impact:** High (eliminates brittle dependencies)  
**Risk:** High (safety-critical code changes)  
**Dependencies:** CRITICAL-001

---

### 🔴 CRITICAL-003: Implement Async Flash Logging

**Current State:** Synchronous flash writes block system for 20-50ms

**Solution:** Double-Buffered Async Queue

```c
// flash_logger_async.h
typedef struct {
    LogEntry_t buffer[2][FLASH_QUEUE_SIZE];  // Double buffer
    volatile uint8_t write_buffer;
    volatile uint8_t read_buffer;
    SemaphoreHandle_t write_sem;
    TaskHandle_t flush_task;
} AsyncFlashLogger_t;

// Returns immediately, queues for background write
bool FlashLogger_QueueEntry(const LogEntry_t* entry);

// Background task flashes when buffer full or timeout
static void Task_FlashFlush(void* pvParameters);
```

**Tasks:**
1. [ ] Design async flash logger API
2. [ ] Implement double-buffered queue
3. [ ] Create background flush task
4. [ ] Add flash wear leveling
5. [ ] Implement power-loss protection (write markers)
6. [ ] Add queue overflow handling
7. [ ] Update existing callers
8. [ ] Add unit tests

**Effort:** 20 hours  
**Impact:** Critical (eliminates blocking)  
**Risk:** High (data loss risk if wrong)  
**Dependencies:** None

---

### 🟠 HIGH-001: Split config.h into Focused Modules

**Current State:** 400+ line god configuration file

**Solution:** Modular Configuration Headers

```
config/
├── config_system.h      // Clock, version
├── config_pwm.h         // PWM parameters
├── config_adc.h         // ADC configuration
├── config_pid.h         // PID tuning
├── config_safety.h      // Safety thresholds
├── config_security.h    // Auth, crypto settings
├── config_debug.h       // Debug options
└── config.h             // Aggregates all (backward compat)
```

**Tasks:**
1. [ ] Create directory structure
2. [ ] Extract clock/version to config_system.h
3. [ ] Extract PWM settings to config_pwm.h
4. [ ] Extract ADC settings to config_adc.h
5. [ ] Extract PID settings to config_pid.h
6. [ ] Extract safety thresholds to config_safety.h
7. [ ] Extract security settings to config_security.h
8. [ ] Update config.h to include all modules
9. [ ] Update include paths in all source files
10. [ ] Update documentation

**Effort:** 8 hours  
**Impact:** Medium (improves maintainability)  
**Risk:** Low (mechanical refactoring)  
**Dependencies:** None

---

### 🟠 HIGH-002: Add Role-Based CLI Access Control

**Current State:** Binary auth (authenticated/not)

**Solution:** Role-Based Access Control (RBAC)

```c
// cli_auth.h
typedef enum {
    ROLE_NONE = 0,
    ROLE_VIEWER,      // Read-only: status, monitor
    ROLE_OPERATOR,    // Standard: pwm, calibrate
    ROLE_MAINTENANCE, // Diagnostics: faults, recovery
    ROLE_ADMIN        // Full: passwd, authstatus
} UserRole_t;

typedef struct {
    const char* name;
    UserRole_t min_role;
    bool requires_auth;
} CommandAccess_t;

// Command table with RBAC
static const CommandAccess_t command_access[] = {
    {"status",      ROLE_NONE,        false},
    {"monitor",     ROLE_VIEWER,      true},
    {"pwm",         ROLE_OPERATOR,    true},
    {"faults",      ROLE_MAINTENANCE, true},
    {"passwd",      ROLE_ADMIN,       true},
    // ...
};
```

**Tasks:**
1. [ ] Define role hierarchy
2. [ ] Create RBAC data structures
3. [ ] Update command registration
4. [ ] Implement role checking
5. [ ] Add user management (optional)
6. [ ] Update help to show required roles
7. [ ] Add unit tests
8. [ ] Update documentation

**Effort:** 12 hours  
**Impact:** High (security improvement)  
**Risk:** Medium (authentication changes)  
**Dependencies:** None

---

### 🟠 HIGH-003: Add Core Module Unit Tests

**Current Coverage:** Only CLI auth, thermal runaway, flash HMAC

**Missing:** HAL, PID, safety, param calc, tasks

**Solution:** Expand Test Suite

```c
// tests/test_pid.c
void test_PID_ProportionalOnly(void);
void test_PID_IntegralWindup(void);
void test_PID_DerivativeOnMeasurement(void);
void test_PID_SetpointWeighting(void);
void test_PID_Saturation(void);

// tests/test_hal_pwm.c (mock HAL)
void test_PWM_Init(void);
void test_PWM_DutyLimits(void);
void test_PWM_Ramping(void);
void test_PWM_EmergencyStop(void);

// tests/test_safety.c
void test_Safety_NormalOperation(void);
void test_Safety_Degradation(void);
void test_Safety_Recovery(void);
void test_Safety_EmergencyStop(void);
```

**Tasks:**
1. [ ] Create mock HAL layer
2. [ ] Write PID controller tests
3. [ ] Write HAL abstraction tests
4. [ ] Write safety system tests
5. [ ] Write parameter calc tests
6. [ ] Write task scheduler tests
7. [ ] Add to CI/CD pipeline
8. [ ] Document testing procedures

**Effort:** 24 hours  
**Impact:** High (improves reliability)  
**Risk:** Low  
**Dependencies:** CRITICAL-001 (needs DI for mocking)

---

### 🟠 HIGH-004: Add CLI Command Timeout

**Current State:** Complex commands can block for 5ms+

**Solution:** Cooperative Multitasking with Yield Points

```c
// cli_commands.c
bool cmd_faults(Adaptive_UART_t* uart, int argc, const char* argv[]) {
    // Check if time budget exceeded
    if (CLI_CheckTimeout()) {
        Adaptive_UART_SendString(uart, "Command timeout, use 'faults --async'\r\n");
        return false;
    }
    
    // For long operations, use async pattern
    if (argc > 1 && strcmp(argv[1], "--async") == 0) {
        return cmd_faults_async(uart);
    }
    
    // Normal execution with yields
    for (int i = 0; i < count; i++) {
        process_entry(i);
        CLI_YieldIfNeeded();  // Yields if time budget exceeded
    }
}
```

**Tasks:**
1. [ ] Define command time budgets
2. [ ] Add CLI_YieldIfNeeded() macro
3. [ ] Identify long-running commands
4. [ ] Add async variants
5. [ ] Implement yield points
6. [ ] Test timing under load
7. [ ] Add documentation

**Effort:** 8 hours  
**Impact:** Medium (RTOS responsiveness)  
**Risk:** Low  
**Dependencies:** None

---

### 🟠 HIGH-005: Evaluate Redundant ADC for Safety

**Current State:** Single ADC for safety-critical measurements

**Solution:** External ADC + Cross-Checking

```c
// hal_adc_redundant.h
typedef struct {
    float internal_adc;   // STM32 ADC1
    float external_adc;   // ADS1115 or similar
    bool cross_check_ok;  // Values agree within threshold
} RedundantMeasurement_t;

bool Adaptive_ADC_GetRedundant(RedundantMeasurement_t* meas);
bool Adaptive_ADC_VerifySafety(RedundantMeasurement_t* meas);
```

**Tasks:**
1. [ ] Research external ADC options
2. [ ] Design cross-checking algorithm
3. [ ] Implement dual ADC driver
4. [ ] Add discrepancy detection
5. [ ] Integrate with safety system
6. [ ] Add fault injection tests
7. [ ] Update safety documentation

**Effort:** 40 hours  
**Impact:** Critical (safety integrity)  
**Risk:** High (hardware changes)  
**Dependencies:** None

---

### 🟠 HIGH-006: Disable JTAG/SWD in Production

**Current State:** Debug interfaces always enabled

**Solution:** Conditional Debug Protection

```c
// config_security.h
#define PRODUCTION_BUILD 0  // Set to 1 for production

#if PRODUCTION_BUILD
    // STM32 option bytes to disable SWD/JTAG
    #define DISABLE_DEBUG_INTERFACES() \
        FLASH_OBProgramInitTypeDef OBInit; \
        HAL_FLASHEx_OBGetConfig(&OBInit); \
        OBInit.OptionType = OPTIONBYTE_USER; \
        OBInit.USERType = OB_USER_SWJ_NJTRST; \
        HAL_FLASHEx_OBProgram(&OBInit);
#endif
```

**Tasks:**
1. [ ] Add production build flag
2. [ ] Implement debug disable code
3. [ ] Add option byte programming
4. [ ] Create production build script
5. [ ] Document recovery procedure (if locked out)
6. [ ] Test on development board first

**Effort:** 8 hours  
**Impact:** High (security)  
**Risk:** High (can lock out device)  
**Dependencies:** None

---

### 🟠 HIGH-007: Refactor High-Complexity Functions

**Functions with Cyclomatic Complexity > 10:**

| Function | Current CC | Target CC |
|----------|------------|-----------|
| EnhancedSafety_Process() | 18 | < 10 |
| CLI_ProcessCommand() | 15 | < 10 |
| PID_Compute() | 12 | < 10 (may keep) |

**Solution:** Extract Helper Functions

```c
// Before: Complex state machine in one function
SafetyState_t EnhancedSafety_Process(EnhancedSafetyManager_t* manager) {
    // 200+ lines with nested switches
}

// After: Delegated state handlers
SafetyState_t EnhancedSafety_Process(EnhancedSafetyManager_t* manager) {
    switch (manager->current_state) {
        case SAFETY_STATE_NORMAL:
            return HandleNormalState(manager);
        case SAFETY_STATE_DEGRADED_PWM:
            return HandleDegradedPWM(manager);
        // ... each state has focused handler
    }
}

static SafetyState_t HandleNormalState(EnhancedSafetyManager_t* manager) {
    // Focused logic, ~50 lines
}
```

**Tasks:**
1. [ ] Analyze EnhancedSafety_Process
2. [ ] Extract state handlers
3. [ ] Analyze CLI_ProcessCommand
4. [ ] Extract command dispatch
5. [ ] Document extracted functions
6. [ ] Verify no behavior change
7. [ ] Update tests

**Effort:** 16 hours  
**Impact:** Medium (maintainability)  
**Risk:** Medium (refactoring safety code)  
**Dependencies:** HIGH-003 (need tests first)

---

### 🟡 MEDIUM-001: Add Const Correctness

**Current State:** Many pointers lack const where appropriate

**Solution:** Systematic const addition

```c
// Before
float PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt);

// After
float PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt);
float PID_ComputeConst(const PID_Controller_t* pid, float setpoint, float measurement, float dt);
// Or separate get/set pattern
```

**Tasks:**
1. [ ] Audit all function signatures
2. [ ] Add const to input-only pointers
3. [ ] Fix compiler warnings
4. [ ] Update headers
5. [ ] Run regression tests

**Effort:** 8 hours  
**Impact:** Low (code quality)  
**Risk:** Low  
**Dependencies:** None

---

### 🟡 MEDIUM-002: Replace Magic Numbers

**Examples:**
```c
// config.h - current
#define ADC_FILTER_IIR_ALPHA 0.1f  // What does 0.1 mean?

// Better
#define ADC_FILTER_IIR_ALPHA_DEFAULT 0.1f
#define ADC_FILTER_TIME_CONSTANT_MS 10  // 10ms @ 100Hz
```

**Tasks:**
1. [ ] Search for magic numbers in source
2. [ ] Document physical meaning
3. [ ] Create named constants
4. [ ] Update comments
5. [ ] Review with domain expert

**Effort:** 4 hours  
**Impact:** Low (readability)  
**Risk:** None  
**Dependencies:** None

---

### 🟡 MEDIUM-003: Add Static Analysis to CI/CD

**Current State:** Cppcheck configured but not automated

**Solution:** GitHub Actions Integration

```yaml
# .github/workflows/static-analysis.yml
name: Static Analysis
on: [push, pull_request]
jobs:
  cppcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install cppcheck
        run: sudo apt-get install cppcheck
      - name: Run cppcheck
        run: |
          cppcheck --enable=all --error-exitcode=1 \
            --suppress=missingIncludeSystem \
            src/ tests/
```

**Tasks:**
1. [ ] Create GitHub Actions workflow
2. [ ] Configure cppcheck rules
3. [ ] Fix existing warnings
4. [ ] Add badge to README
5. [ ] Block PRs on failure

**Effort:** 4 hours  
**Impact:** Medium (quality gate)  
**Risk:** Low  
**Dependencies:** None

---

### 🟡 MEDIUM-004: Implement Hardware-in-Loop Testing

**Solution:** Renode or QEMU Integration

```python
# tests/hil/test_pwm_output.py (Renode)
def test_pwm_frequency():
    # Start Renode with STM32F401
    renode.start("scripts/single-node/stm32f401.resc")
    
    # Load firmware
    renode.load_elf("build/AdaptivePWM.elf")
    
    # Start emulation
    renode.start_emulation()
    
    # Check PWM frequency via virtual scope
    freq = renode.get_pwm_frequency("TIM1")
    assert 19000 < freq < 21000  # 20kHz ± 5%
```

**Tasks:**
1. [ ] Evaluate Renode vs QEMU
2. [ ] Set up HIL framework
3. [ ] Create basic smoke tests
4. [ ] Add safety system HIL tests
5. [ ] Integrate with CI/CD

**Effort:** 16 hours  
**Impact:** High (testing coverage)  
**Risk:** Low  
**Dependencies:** None

---

### 🟢 LOW-001: Improve Code Comments

**Tasks:**
1. [ ] Add "why" comments (not just "what")
2. [ ] Document non-obvious algorithms
3. [ ] Add references to datasheets
4. [ ] Document timing assumptions

**Effort:** 8 hours  
**Impact:** Low  
**Risk:** None  
**Dependencies:** None

---

### 🟢 LOW-002: Optimize RAM Usage

**Current RAM:** ~17 KB used of 96 KB available

**Opportunities:**
- Reduce stack sizes (currently generous)
- Pack structures where alignment not critical
- Move constants to flash (const data)

**Tasks:**
1. [ ] Profile actual stack usage
2. [ ] Review structure packing
3. [ ] Add `const` to lookup tables
4. [ ] Verify with memory map

**Effort:** 8 hours  
**Impact:** Low (78% free, not urgent)  
**Risk:** Low  
**Dependencies:** None

---

## Implementation Roadmap

### Phase 1: Foundation (Sprint 1-2, Weeks 1-4)

**Goal:** Establish architectural foundation for future improvements

| Week | Task | Owner | Deliverable |
|------|------|-------|-------------|
| 1 | CRITICAL-001 | Dev A | Context struct implemented |
| 1 | CRITICAL-002 | Dev B | Event system prototype |
| 2 | CRITICAL-003 | Dev A | Async flash logger |
| 2 | HIGH-001 | Dev B | Split config.h |
| 2 | HIGH-003 (start) | Dev A | Mock HAL + PID tests |

**Phase 1 Success Criteria:**
- [ ] No global state (except context pointer)
- [ ] No circular dependencies
- [ ] Flash logging is non-blocking
- [ ] >50% unit test coverage

---

### Phase 2: Security & Safety (Sprint 3-4, Weeks 5-8)

**Goal:** Harden system for production

| Week | Task | Owner | Deliverable |
|------|------|-------|-------------|
| 3 | HIGH-002 | Dev A | RBAC implemented |
| 3 | HIGH-006 | Dev B | JTAG disable feature |
| 4 | HIGH-005 (research) | Dev A | ADC selection report |
| 4 | HIGH-007 | Dev B | Refactored safety functions |
| 4 | MEDIUM-003 | Dev A | CI/CD static analysis |

**Phase 2 Success Criteria:**
- [ ] Role-based access control
- [ ] Production build disables debug
- [ ] All functions CC < 10
- [ ] Automated static analysis

---

### Phase 3: Testing & Validation (Sprint 5-6, Weeks 9-12)

**Goal:** Comprehensive test coverage

| Week | Task | Owner | Deliverable |
|------|------|-------|-------------|
| 5 | MEDIUM-004 | Dev A | HIL framework |
| 5 | HIGH-003 (complete) | Dev B | Full unit test coverage |
| 6 | HIGH-004 | Dev A | CLI timeouts |
| 6 | HIGH-005 (implement) | Dev B | Redundant ADC (if approved) |

**Phase 3 Success Criteria:**
- [ ] >80% code coverage
- [ ] HIL tests passing
- [ ] CLI never blocks >1ms

---

### Phase 4: Polish (Sprint 7+, Week 13+)

**Goal:** Technical debt reduction and optimization

| Task | Priority | Owner |
|------|----------|-------|
| MEDIUM-001 | 🟡 | Dev A |
| MEDIUM-002 | 🟡 | Dev B |
| LOW-001 | 🟢 | Dev A |
| LOW-002 | 🟢 | Dev B |
| Documentation updates | 🟡 | Both |

---

## Resource Requirements

### Time Estimates

| Priority | Total Hours | Developers | Duration |
|----------|-------------|------------|----------|
| Critical | 48 hours | 2 | 2 weeks |
| High | 108 hours | 2 | 4 weeks |
| Medium | 32 hours | 1-2 | 2 weeks |
| Low | 16 hours | 1 | Ongoing |
| **Total** | **204 hours** | **2 FTE** | **12 weeks** |

### Skills Required

- **Embedded C** (expert) - All tasks
- **RTOS/FreeRTOS** (advanced) - Task-related
- **Security** (intermediate) - Auth, JTAG
- **Hardware** (intermediate) - ADC, flash
- **Testing** (advanced) - Unit tests, HIL

### Tools/Licenses

| Tool | Cost | For Task |
|------|------|----------|
| PC-lint Plus | ~$300 | Static analysis |
| Renode | Free | HIL testing |
| STM32CubeIDE | Free | Development |
| PlatformIO | Free | Build system |

---

## Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Regression in safety code | Medium | Critical | Extensive testing, HIL validation |
| Device lockout (JTAG) | Low | High | Test on spare board first |
| Schedule slip | Medium | Medium | Prioritize critical items |
| Hardware ADC unavailable | Low | Medium | Evaluate alternatives early |
| Test complexity | Medium | Medium | Invest in mock HAL |

---

## Success Metrics

| Metric | Current | Target | Phase |
|--------|---------|--------|-------|
| Global state variables | 8+ | 1 (context) | Phase 1 |
| Circular dependencies | 3 | 0 | Phase 1 |
| Flash write blocking | 50ms | 0ms | Phase 1 |
| Unit test coverage | 15% | 80% | Phase 3 |
| Cyclomatic complexity >10 | 3 | 0 | Phase 2 |
| CI build time | N/A | <5 min | Phase 2 |
| Static analysis warnings | ? | 0 | Phase 2 |

---

## Review Schedule

| Review | Date | Focus |
|--------|------|-------|
| Phase 1 Review | +2 weeks | Architecture changes |
| Phase 2 Review | +4 weeks | Security hardening |
| Phase 3 Review | +8 weeks | Testing coverage |
| Final Review | +12 weeks | Release readiness |

---

**End of Improvement Plan**
