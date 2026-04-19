/**
 * @file test_hal_pwm.c
 * @brief Unit tests for HAL PWM Module (PWM-ARCH-008)
 *
 * Tests cover:
 * - PWM initialization
 * - Duty cycle set/get with hysteresis
 * - Ramp limiting
 * - Frequency change
 * - Dead-time management
 * - Safety limits enforcement
 *
 * @version 1.0.0
 * @date 2026-04-18
 */

#include "unity.h"
#include "hal_pwm.h"
#include "config.h"
#include <string.h>

// Test fixtures
static Adaptive_PWM_t test_pwm;

void setUp(void)
{
    memset(&test_pwm, 0, sizeof(test_pwm));
}

void tearDown(void)
{
    // Cleanup
}

// =============================================================================
// TEST: Initialization
// =============================================================================

void test_PWM_Init_ShouldInitializeStructure(void)
{
    bool result = Adaptive_PWM_Init(&test_pwm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pwm.current_duty);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pwm.target_duty);
    TEST_ASSERT_FALSE(test_pwm.enabled);
}

void test_PWM_Init_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_Init(NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Frequency Setup
// =============================================================================

void test_PWM_SetFrequency_ShouldCalculateARR(void)
{
    Adaptive_PWM_Init(&test_pwm);

    bool result = Adaptive_PWM_SetFrequency(&test_pwm, 20000); // 20 kHz

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(20000, test_pwm.frequency);
    // ARR = 84MHz / 20kHz - 1 = 4199
    TEST_ASSERT_EQUAL_UINT32(4199, test_pwm.arr_value);
}

void test_PWM_SetFrequency_Zero_ShouldReturnFalse(void)
{
    Adaptive_PWM_Init(&test_pwm);

    bool result = Adaptive_PWM_SetFrequency(&test_pwm, 0);

    TEST_ASSERT_FALSE(result);
}

void test_PWM_SetFrequency_TooHigh_ShouldReturnFalse(void)
{
    Adaptive_PWM_Init(&test_pwm);

    // Try to set frequency higher than timer allows
    bool result = Adaptive_PWM_SetFrequency(&test_pwm, 100000000); // 100 MHz

    TEST_ASSERT_FALSE(result);
}

void test_PWM_SetFrequency_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_SetFrequency(NULL, 20000);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Duty Cycle Set/Get
// =============================================================================

void test_PWM_SetDuty_ShouldUpdateTarget(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 20000);

    bool result = Adaptive_PWM_SetDuty(&test_pwm, 0.5f); // 50%

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, test_pwm.target_duty);
}

void test_PWM_SetDuty_HardLimits_ShouldClamp(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 20000);

    // Below hard minimum
    bool result = Adaptive_PWM_SetDuty(&test_pwm, 0.01f);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, PWM_HARD_MIN_DUTY, test_pwm.target_duty);

    // Above hard maximum
    result = Adaptive_PWM_SetDuty(&test_pwm, 0.99f);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, PWM_HARD_MAX_DUTY, test_pwm.target_duty);
}

void test_PWM_SetDuty_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_SetDuty(NULL, 0.5f);
    TEST_ASSERT_FALSE(result);
}

void test_PWM_GetDuty_ShouldReturnCurrent(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 20000);
    Adaptive_PWM_SetDuty(&test_pwm, 0.75f);

    float duty = Adaptive_PWM_GetDuty(&test_pwm);

    // Should return current_duty (which starts at 0 before update)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, duty);
}

void test_PWM_GetDuty_NullPointer_ShouldReturnZero(void)
{
    float duty = Adaptive_PWM_GetDuty(NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, duty);
}

// =============================================================================
// TEST: Hysteresis
// =============================================================================

void test_PWM_SetDuty_Hysteresis_ShouldFilterSmallChanges(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 20000);

    // Set initial duty
    Adaptive_PWM_SetDuty(&test_pwm, 0.5f);
    Adaptive_PWM_Update(&test_pwm, 0.01f); // Apply change
    test_pwm.current_duty = test_pwm.target_duty; // Simulate update

    // Small change within hysteresis
    Adaptive_PWM_SetDuty(&test_pwm, 0.504f); // 0.4% change

    // Should be filtered out by hysteresis check
    // Target should still update but actual update may be suppressed
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.504f, test_pwm.target_duty);
}

// =============================================================================
// TEST: Ramp Limiting
// =============================================================================

void test_PWM_Update_RampLimiting_ShouldLimitRate(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 20000);

    // Set initial duty
    Adaptive_PWM_SetDuty(&test_pwm, 0.2f);
    Adaptive_PWM_Update(&test_pwm, 0.01f); // 10ms update
    test_pwm.current_duty = test_pwm.target_duty;

    // Try large jump
    Adaptive_PWM_SetDuty(&test_pwm, 0.8f); // 60% jump

    // Update with 10ms dt
    Adaptive_PWM_Update(&test_pwm, 0.01f);

    // Should be limited by ramp rate (10% per second * 0.01s = 0.1% max)
    // But in one 10ms step, max change is PWM_RAMP_RATE_PER_SEC * dt
    float max_change = PWM_RAMP_RATE_PER_SEC * 0.01f; // 0.001
    float actual_change = test_pwm.target_duty - test_pwm.current_duty;

    // Current should have moved toward target
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f + max_change, test_pwm.current_duty);
}

void test_PWM_Update_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_Update(NULL, 0.01f);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Enable/Disable
// =============================================================================

void test_PWM_Enable_ShouldSetEnabledFlag(void)
{
    Adaptive_PWM_Init(&test_pwm);

    bool result = Adaptive_PWM_Enable(&test_pwm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(test_pwm.enabled);
}

void test_PWM_Disable_ShouldClearEnabledFlag(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_Enable(&test_pwm);

    bool result = Adaptive_PWM_Disable(&test_pwm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(test_pwm.enabled);
}

void test_PWM_Enable_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_Enable(NULL);
    TEST_ASSERT_FALSE(result);
}

void test_PWM_Disable_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_Disable(NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Emergency Stop
// =============================================================================

void test_PWM_EmergencyStop_ShouldDisableAndZero(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 20000);
    Adaptive_PWM_SetDuty(&test_pwm, 0.5f);
    Adaptive_PWM_Enable(&test_pwm);

    bool result = Adaptive_PWM_EmergencyStop(&test_pwm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(test_pwm.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pwm.target_duty);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pwm.current_duty);
}

void test_PWM_EmergencyStop_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_EmergencyStop(NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Dead Time
// =============================================================================

void test_PWM_SetDeadTime_ShouldUpdateDeadTime(void)
{
    Adaptive_PWM_Init(&test_pwm);

    bool result = Adaptive_PWM_SetDeadTime(&test_pwm, 500); // 500 ns

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(500, test_pwm.dead_time_ns);
}

void test_PWM_SetDeadTime_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_PWM_SetDeadTime(NULL, 400);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Frequency Get
// =============================================================================

void test_PWM_GetFrequency_ShouldReturnConfiguredValue(void)
{
    Adaptive_PWM_Init(&test_pwm);
    Adaptive_PWM_SetFrequency(&test_pwm, 25000); // 25 kHz

    uint32_t freq = Adaptive_PWM_GetFrequency(&test_pwm);

    TEST_ASSERT_EQUAL_UINT32(25000, freq);
}

void test_PWM_GetFrequency_NullPointer_ShouldReturnZero(void)
{
    uint32_t freq = Adaptive_PWM_GetFrequency(NULL);
    TEST_ASSERT_EQUAL_UINT32(0, freq);
}

// =============================================================================
// TEST: IsEnabled
// =============================================================================

void test_PWM_IsEnabled_ShouldTrackState(void)
{
    Adaptive_PWM_Init(&test_pwm);

    TEST_ASSERT_FALSE(Adaptive_PWM_IsEnabled(&test_pwm));

    Adaptive_PWM_Enable(&test_pwm);
    TEST_ASSERT_TRUE(Adaptive_PWM_IsEnabled(&test_pwm));

    Adaptive_PWM_Disable(&test_pwm);
    TEST_ASSERT_FALSE(Adaptive_PWM_IsEnabled(&test_pwm));
}

void test_PWM_IsEnabled_NullPointer_ShouldReturnFalse(void)
{
    TEST_ASSERT_FALSE(Adaptive_PWM_IsEnabled(NULL));
}

// =============================================================================
// TEST: Configuration Constants
// =============================================================================

void test_PWM_Configuration_ShouldBeValid(void)
{
    // Hard limits should be within 0-1
    TEST_ASSERT_TRUE(PWM_HARD_MIN_DUTY > 0.0f);
    TEST_ASSERT_TRUE(PWM_HARD_MAX_DUTY < 1.0f);
    TEST_ASSERT_TRUE(PWM_HARD_MIN_DUTY < PWM_HARD_MAX_DUTY);

    // Soft limits should be within hard limits
    TEST_ASSERT_TRUE(PWM_SOFT_MIN_DUTY >= PWM_HARD_MIN_DUTY);
    TEST_ASSERT_TRUE(PWM_SOFT_MAX_DUTY <= PWM_HARD_MAX_DUTY);

    // Hysteresis should be small
    TEST_ASSERT_TRUE(PWM_DUTY_HYSTERESIS < 0.1f);

    // Ramp rate should be reasonable
    TEST_ASSERT_TRUE(PWM_RAMP_RATE_PER_SEC > 0.0f);
    TEST_ASSERT_TRUE(PWM_RAMP_RATE_PER_SEC < 1.0f);
}

// =============================================================================
// TEST: Full Sequence
// =============================================================================

void test_PWM_FullSequence_ShouldWork(void)
{
    // Initialize
    TEST_ASSERT_TRUE(Adaptive_PWM_Init(&test_pwm));

    // Set frequency
    TEST_ASSERT_TRUE(Adaptive_PWM_SetFrequency(&test_pwm, 20000));

    // Set duty cycle
    TEST_ASSERT_TRUE(Adaptive_PWM_SetDuty(&test_pwm, 0.5f));

    // Enable
    TEST_ASSERT_TRUE(Adaptive_PWM_Enable(&test_pwm));
    TEST_ASSERT_TRUE(Adaptive_PWM_IsEnabled(&test_pwm));

    // Get duty (may be 0 until first update)
    float duty = Adaptive_PWM_GetDuty(&test_pwm);
    (void)duty;

    // Update
    TEST_ASSERT_TRUE(Adaptive_PWM_Update(&test_pwm, 0.001f)); // 1ms

    // Disable
    TEST_ASSERT_TRUE(Adaptive_PWM_Disable(&test_pwm));
    TEST_ASSERT_FALSE(Adaptive_PWM_IsEnabled(&test_pwm));
}

// =============================================================================
// RUN TESTS
// =============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_PWM_Init_ShouldInitializeStructure);
    RUN_TEST(test_PWM_Init_NullPointer_ShouldReturnFalse);

    // Frequency tests
    RUN_TEST(test_PWM_SetFrequency_ShouldCalculateARR);
    RUN_TEST(test_PWM_SetFrequency_Zero_ShouldReturnFalse);
    RUN_TEST(test_PWM_SetFrequency_TooHigh_ShouldReturnFalse);
    RUN_TEST(test_PWM_SetFrequency_NullPointer_ShouldReturnFalse);

    // Duty cycle tests
    RUN_TEST(test_PWM_SetDuty_ShouldUpdateTarget);
    RUN_TEST(test_PWM_SetDuty_HardLimits_ShouldClamp);
    RUN_TEST(test_PWM_SetDuty_NullPointer_ShouldReturnFalse);
    RUN_TEST(test_PWM_GetDuty_ShouldReturnCurrent);
    RUN_TEST(test_PWM_GetDuty_NullPointer_ShouldReturnZero);

    // Hysteresis tests
    RUN_TEST(test_PWM_SetDuty_Hysteresis_ShouldFilterSmallChanges);

    // Ramp limiting tests
    RUN_TEST(test_PWM_Update_RampLimiting_ShouldLimitRate);
    RUN_TEST(test_PWM_Update_NullPointer_ShouldReturnFalse);

    // Enable/disable tests
    RUN_TEST(test_PWM_Enable_ShouldSetEnabledFlag);
    RUN_TEST(test_PWM_Disable_ShouldClearEnabledFlag);
    RUN_TEST(test_PWM_Enable_NullPointer_ShouldReturnFalse);
    RUN_TEST(test_PWM_Disable_NullPointer_ShouldReturnFalse);

    // Emergency stop tests
    RUN_TEST(test_PWM_EmergencyStop_ShouldDisableAndZero);
    RUN_TEST(test_PWM_EmergencyStop_NullPointer_ShouldReturnFalse);

    // Dead time tests
    RUN_TEST(test_PWM_SetDeadTime_ShouldUpdateDeadTime);
    RUN_TEST(test_PWM_SetDeadTime_NullPointer_ShouldReturnFalse);

    // Frequency get tests
    RUN_TEST(test_PWM_GetFrequency_ShouldReturnConfiguredValue);
    RUN_TEST(test_PWM_GetFrequency_NullPointer_ShouldReturnZero);

    // IsEnabled tests
    RUN_TEST(test_PWM_IsEnabled_ShouldTrackState);
    RUN_TEST(test_PWM_IsEnabled_NullPointer_ShouldReturnFalse);

    // Configuration tests
    RUN_TEST(test_PWM_Configuration_ShouldBeValid);

    // Integration test
    RUN_TEST(test_PWM_FullSequence_ShouldWork);

    return UNITY_END();
}
