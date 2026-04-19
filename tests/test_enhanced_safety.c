/**
 * @file test_enhanced_safety.c
 * @brief Unit tests for Enhanced Safety Module (PWM-ARCH-008)
 *
 * Tests cover:
 * - Safety state machine transitions
 * - Voltage/current/temperature limits
 * - Graceful degradation
 * - Recovery attempts
 * - Watchdog interaction
 *
 * @version 1.0.0
 * @date 2026-04-18
 */

#include "unity.h"
#include "enhanced_safety.h"
#include "config.h"
#include <string.h>

// Mock tick
uint32_t mock_tick_safety = 0;
uint32_t HAL_GetTick(void) { return mock_tick_safety; }

// Test fixtures
static Enhanced_Safety_t test_safety;

void setUp(void)
{
    mock_tick_safety = 0;
    memset(&test_safety, 0, sizeof(test_safety));
}

void tearDown(void)
{
    // Cleanup
}

// =============================================================================
// TEST: Initialization
// =============================================================================

void test_EnhancedSafety_Init_ShouldInitializeState(void)
{
    bool result = EnhancedSafety_Init(&test_safety);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_INIT, test_safety.state);
    TEST_ASSERT_FALSE(test_safety.fault_active);
    TEST_ASSERT_EQUAL_UINT32(0, test_safety.fault_count);
}

void test_EnhancedSafety_Init_NullPointer_ShouldReturnFalse(void)
{
    bool result = EnhancedSafety_Init(NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: State Machine - Normal Transitions
// =============================================================================

void test_EnhancedSafety_Update_InitToRunning(void)
{
    EnhancedSafety_Init(&test_safety);

    // Provide valid operating conditions
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_OK, status);
    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_RUNNING, test_safety.state);
}

void test_EnhancedSafety_Update_MaintainsRunning(void)
{
    EnhancedSafety_Init(&test_safety);

    // Transition to running
    EnhancedSafety_Update(&test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Continue with valid conditions
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 30.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_OK, status);
    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_RUNNING, test_safety.state);
}

// =============================================================================
// TEST: Voltage Limits
// =============================================================================

void test_EnhancedSafety_Update_OverVoltage_ShouldFault(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(&test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f); // Enter running

    // Over-voltage condition
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_OVERVOLTAGE, status);
    TEST_ASSERT_TRUE(test_safety.fault_active);
}

void test_EnhancedSafety_Update_UnderVoltage_ShouldFault(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Under-voltage condition
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 2.0f, 2.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_UNDERVOLTAGE, status);
    TEST_ASSERT_TRUE(test_safety.fault_active);
}

void test_EnhancedSafety_Update_InputUnderVoltage_ShouldFault(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Input under-voltage
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 3.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_UNDERVOLTAGE, status);
}

// =============================================================================
// TEST: Current Limits
// =============================================================================

void test_EnhancedSafety_Update_OverCurrent_ShouldFault(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Over-current condition
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 15.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_OVERCURRENT, status);
    TEST_ASSERT_TRUE(test_safety.fault_active);
}

void test_EnhancedSafety_Update_WarningCurrent_ShouldWarn(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Warning current level
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 9.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_WARNING, status);
    TEST_ASSERT_FALSE(test_safety.fault_active);
}

// =============================================================================
// TEST: Temperature Limits
// =============================================================================

void test_EnhancedSafety_Update_OverTempWarning_ShouldWarn(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Warning temperature
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 80.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_WARNING, status);
}

void test_EnhancedSafety_Update_OverTempCritical_ShouldFault(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Critical temperature
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 90.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_OVERTEMP, status);
    TEST_ASSERT_TRUE(test_safety.fault_active);
}

void test_EnhancedSafety_Update_ShutdownTemp_ShouldShutdown(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Shutdown temperature
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 100.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_OVERTEMP, status);
}

// =============================================================================
// TEST: Fault Clearing
// =============================================================================

void test_EnhancedSafety_ClearFault_ShouldResetFault(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f); // Fault

    TEST_ASSERT_TRUE(test_safety.fault_active);

    bool result = EnhancedSafety_ClearFault(&test_safety);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(test_safety.fault_active);
}

void test_EnhancedSafety_ClearFault_NoFault_ShouldReturnFalse(void)
{
    EnhancedSafety_Init(&test_safety);

    bool result = EnhancedSafety_ClearFault(&test_safety);

    TEST_ASSERT_FALSE(result); // No fault to clear
}

void test_EnhancedSafety_ClearFault_NullPointer_ShouldReturnFalse(void)
{
    bool result = EnhancedSafety_ClearFault(NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: State Machine Transitions
// =============================================================================

void test_EnhancedSafety_Update_FaultToFaultState(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f); // Fault

    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_FAULT, test_safety.state);
}

void test_EnhancedSafety_Update_RecoveryAttempt(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f); // Fault

    // Clear fault and try recovery
    EnhancedSafety_ClearFault(&test_safety);

    // Recovery attempt with valid conditions
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Should be OK or RECOVERY depending on implementation
    TEST_ASSERT_TRUE(status == SAFETY_OK || status == SAFETY_RECOVERING);
}

// =============================================================================
// TEST: Null Pointer Handling
// =============================================================================

void test_EnhancedSafety_Update_NullPointer_ShouldReturnFault(void)
{
    Safety_Status_t status = EnhancedSafety_Update(
        NULL, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_INTERNAL, status);
}

// =============================================================================
// TEST: Get State
// =============================================================================

void test_EnhancedSafety_GetState_ShouldReturnCurrentState(void)
{
    EnhancedSafety_Init(&test_safety);

    Safety_State_t state = EnhancedSafety_GetState(&test_safety);
    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_INIT, state);

    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    state = EnhancedSafety_GetState(&test_safety);
    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_RUNNING, state);
}

void test_EnhancedSafety_GetState_NullPointer_ShouldReturnError(void)
{
    Safety_State_t state = EnhancedSafety_GetState(NULL);
    TEST_ASSERT_EQUAL_INT(SAFETY_STATE_ERROR, state);
}

// =============================================================================
// TEST: Is Fault Active
// =============================================================================

void test_EnhancedSafety_IsFaultActive_ShouldTrackFaults(void)
{
    EnhancedSafety_Init(&test_safety);

    TEST_ASSERT_FALSE(EnhancedSafety_IsFaultActive(&test_safety));

    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f); // Fault

    TEST_ASSERT_TRUE(EnhancedSafety_IsFaultActive(&test_safety));
}

void test_EnhancedSafety_IsFaultActive_NullPointer_ShouldReturnFalse(void)
{
    TEST_ASSERT_FALSE(EnhancedSafety_IsFaultActive(NULL));
}

// =============================================================================
// TEST: Get Fault Count
// =============================================================================

void test_EnhancedSafety_GetFaultCount_ShouldCountFaults(void)
{
    EnhancedSafety_Init(&test_safety);

    TEST_ASSERT_EQUAL_UINT32(0, EnhancedSafety_GetFaultCount(&test_safety));

    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f); // Fault

    TEST_ASSERT_EQUAL_UINT32(1, EnhancedSafety_GetFaultCount(&test_safety));

    EnhancedSafety_ClearFault(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 2.0f, 25.0f, 0.5f); // Another fault

    TEST_ASSERT_EQUAL_UINT32(2, EnhancedSafety_GetFaultCount(&test_safety));
}

void test_EnhancedSafety_GetFaultCount_NullPointer_ShouldReturnZero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, EnhancedSafety_GetFaultCount(NULL));
}

// =============================================================================
// TEST: Get Status String
// =============================================================================

void test_EnhancedSafety_GetStatusString_ShouldReturnString(void)
{
    EnhancedSafety_Init(&test_safety);

    const char* str = EnhancedSafety_GetStatusString(SAFETY_OK);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT_TRUE(strlen(str) > 0);

    str = EnhancedSafety_GetStatusString(SAFETY_FAULT_OVERVOLTAGE);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT_TRUE(strlen(str) > 0);
}

// =============================================================================
// TEST: Multiple Fault Conditions
// =============================================================================

void test_EnhancedSafety_Update_MultipleFaults_Priority(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Multiple simultaneous faults: overvoltage + overcurrent + overtemp
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, 35.0f, 15.0f, 100.0f, 0.5f);

    // Should report one of the faults (priority order implementation dependent)
    TEST_ASSERT_TRUE(
        status == SAFETY_FAULT_OVERVOLTAGE ||
        status == SAFETY_FAULT_OVERCURRENT ||
        status == SAFETY_FAULT_OVERTEMP ||
        status == SAFETY_FAULT_MULTIPLE);
}

// =============================================================================
// TEST: Boundary Conditions
// =============================================================================

void test_EnhancedSafety_Update_ExactlyAtLimit(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Exactly at voltage limit
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, VOLTAGE_MAX_V, 2.0f, 25.0f, 0.5f);

    // Should be OK or WARNING depending on implementation
    TEST_ASSERT_TRUE(status == SAFETY_OK || status == SAFETY_WARNING);
}

void test_EnhancedSafety_Update_JustOverLimit(void)
{
    EnhancedSafety_Init(&test_safety);
    EnhancedSafety_Update(
        &test_safety, 12.0f, 5.0f, 2.0f, 25.0f, 0.5f);

    // Just over limit
    Safety_Status_t status = EnhancedSafety_Update(
        &test_safety, 12.0f, VOLTAGE_MAX_V + 0.1f, 2.0f, 25.0f, 0.5f);

    TEST_ASSERT_EQUAL_INT(SAFETY_FAULT_OVERVOLTAGE, status);
}

// =============================================================================
// RUN TESTS
// =============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_EnhancedSafety_Init_ShouldInitializeState);
    RUN_TEST(test_EnhancedSafety_Init_NullPointer_ShouldReturnFalse);

    // State machine tests
    RUN_TEST(test_EnhancedSafety_Update_InitToRunning);
    RUN_TEST(test_EnhancedSafety_Update_MaintainsRunning);
    RUN_TEST(test_EnhancedSafety_Update_FaultToFaultState);

    // Voltage limit tests
    RUN_TEST(test_EnhancedSafety_Update_OverVoltage_ShouldFault);
    RUN_TEST(test_EnhancedSafety_Update_UnderVoltage_ShouldFault);
    RUN_TEST(test_EnhancedSafety_Update_InputUnderVoltage_ShouldFault);

    // Current limit tests
    RUN_TEST(test_EnhancedSafety_Update_OverCurrent_ShouldFault);
    RUN_TEST(test_EnhancedSafety_Update_WarningCurrent_ShouldWarn);

    // Temperature tests
    RUN_TEST(test_EnhancedSafety_Update_OverTempWarning_ShouldWarn);
    RUN_TEST(test_EnhancedSafety_Update_OverTempCritical_ShouldFault);
    RUN_TEST(test_EnhancedSafety_Update_ShutdownTemp_ShouldShutdown);

    // Fault clearing tests
    RUN_TEST(test_EnhancedSafety_ClearFault_ShouldResetFault);
    RUN_TEST(test_EnhancedSafety_ClearFault_NoFault_ShouldReturnFalse);
    RUN_TEST(test_EnhancedSafety_ClearFault_NullPointer_ShouldReturnFalse);

    // Recovery tests
    RUN_TEST(test_EnhancedSafety_Update_RecoveryAttempt);

    // Null pointer tests
    RUN_TEST(test_EnhancedSafety_Update_NullPointer_ShouldReturnFault);

    // Get state tests
    RUN_TEST(test_EnhancedSafety_GetState_ShouldReturnCurrentState);
    RUN_TEST(test_EnhancedSafety_GetState_NullPointer_ShouldReturnError);

    // Is fault active tests
    RUN_TEST(test_EnhancedSafety_IsFaultActive_ShouldTrackFaults);
    RUN_TEST(test_EnhancedSafety_IsFaultActive_NullPointer_ShouldReturnFalse);

    // Fault count tests
    RUN_TEST(test_EnhancedSafety_GetFaultCount_ShouldCountFaults);
    RUN_TEST(test_EnhancedSafety_GetFaultCount_NullPointer_ShouldReturnZero);

    // Status string tests
    RUN_TEST(test_EnhancedSafety_GetStatusString_ShouldReturnString);

    // Multiple faults tests
    RUN_TEST(test_EnhancedSafety_Update_MultipleFaults_Priority);

    // Boundary tests
    RUN_TEST(test_EnhancedSafety_Update_ExactlyAtLimit);
    RUN_TEST(test_EnhancedSafety_Update_JustOverLimit);

    return UNITY_END();
}
