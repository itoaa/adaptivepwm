/**
 * @file test_pid.c
 * @brief Unit tests for PID Controller Module (PWM-ARCH-008)
 *
 * Tests cover:
 * - PID initialization
 * - Basic proportional control
 * - Integral windup prevention
 * - Derivative filtering
 * - Setpoint weighting
 * - Output limiting
 * - Reset functionality
 *
 * @version 1.0.0
 * @date 2026-04-18
 */

#include "unity.h"
#include "config.h"
#include <string.h>
#include <math.h>

// PID controller structure from config.h
typedef struct {
    float Kp, Ki, Kd;          // Gains
    float setpoint_weight;       // Setpoint weighting (0-1)
    float derivative_filter;       // Low-pass filter for derivative (0-1)
    float integral;              // Integral accumulator
    float integral_min, integral_max; // Anti-windup limits
    float prev_error;            // Previous error for derivative
    float prev_measurement;      // Previous measurement (derivative on measurement)
    float output_min, output_max; // Output limits
    float d_filtered_prev;       // PWM-ARCH-003: Instance-based derivative filter state
    bool initialized;          // First run flag
} PID_Controller_t;

// External declarations
void PID_Init(PID_Controller_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max);
float PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt);
void PID_Reset(PID_Controller_t* pid);
void PID_SetGains(PID_Controller_t* pid, float Kp, float Ki, float Kd);
void PID_SetSetpointWeight(PID_Controller_t* pid, float weight);
void PID_SetDerivativeFilter(PID_Controller_t* pid, float alpha);
float PID_GetIntegral(const PID_Controller_t* pid);
void PID_SetIntegral(PID_Controller_t* pid, float integral);

// Test fixtures
static PID_Controller_t test_pid;

void setUp(void)
{
    memset(&test_pid, 0, sizeof(test_pid));
}

void tearDown(void)
{
    // Cleanup
}

// =============================================================================
// TEST: Initialization
// =============================================================================

void test_PID_Init_ShouldInitializeController(void)
{
    PID_Init(&test_pid, 1.0f, 0.5f, 0.1f, 0.0f, 1.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, test_pid.Kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, test_pid.Ki);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, test_pid.Kd);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pid.prev_error);
    TEST_ASSERT_TRUE(test_pid.initialized);
}

void test_PID_Init_ShouldSetLimits(void)
{
    PID_Init(&test_pid, 1.0f, 0.5f, 0.1f, -1.0f, 1.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, test_pid.output_min);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, test_pid.output_max);
}

void test_PID_Init_NullPointer_ShouldNotCrash(void)
{
    // Should not crash
    PID_Init(NULL, 1.0f, 0.5f, 0.1f, 0.0f, 1.0f);
    TEST_ASSERT_TRUE(1);
}

// =============================================================================
// TEST: Proportional Control
// =============================================================================

void test_PID_Compute_Proportional_ShouldProduceOutput(void)
{
    PID_Init(&test_pid, 2.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    // Error = 10 - 5 = 5
    // Output = Kp * error = 2 * 5 = 10
    float output = PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, output);
}

void test_PID_Compute_ZeroError_ShouldProduceZeroOutput(void)
{
    PID_Init(&test_pid, 2.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    // Error = 0
    float output = PID_Compute(&test_pid, 5.0f, 5.0f, 0.01f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, output);
}

void test_PID_Compute_NegativeError_ShouldProduceNegativeOutput(void)
{
    PID_Init(&test_pid, 2.0f, 0.0f, 0.0f, -100.0f, 100.0f);

    // Error = 5 - 10 = -5
    // Output = Kp * error = 2 * -5 = -10
    float output = PID_Compute(&test_pid, 5.0f, 10.0f, 0.01f);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, -10.0f, output);
}

void test_PID_Compute_NullPointer_ShouldReturnZero(void)
{
    float output = PID_Compute(NULL, 10.0f, 5.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output);
}

// =============================================================================
// TEST: Integral Control
// =============================================================================

void test_PID_Compute_Integral_ShouldAccumulate(void)
{
    PID_Init(&test_pid, 0.0f, 1.0f, 0.0f, 0.0f, 100.0f);

    // First call
    float output1 = PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    // integral = Ki * error * dt = 1 * 5 * 0.01 = 0.05
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.05f, output1);

    // Second call with same error
    float output2 = PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    // integral = 0.05 + 1 * 5 * 0.01 = 0.10
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.10f, output2);

    // Third call
    float output3 = PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    // integral = 0.10 + 1 * 5 * 0.01 = 0.15
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.15f, output3);
}

// =============================================================================
// TEST: Anti-Windup
// =============================================================================

void test_PID_Compute_AntiWindup_ShouldLimitIntegral(void)
{
    PID_Init(&test_pid, 1.0f, 10.0f, 0.0f, 0.0f, 1.0f);

    // Large error should saturate output
    for (int i = 0; i < 100; i++) {
        PID_Compute(&test_pid, 100.0f, 0.0f, 0.01f);
    }

    // Integral should be limited
    float integral = PID_GetIntegral(&test_pid);
    TEST_ASSERT_LESS_THAN_FLOAT(20.0f, integral); // Should be clamped
}

// =============================================================================
// TEST: Derivative Control
// =============================================================================

void test_PID_Compute_Derivative_ShouldRespondToChange(void)
{
    PID_Init(&test_pid, 0.0f, 0.0f, 1.0f, -100.0f, 100.0f);

    // First call - derivative is 0 (no previous measurement)
    float output1 = PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, output1);

    // Second call - measurement changed from 5 to 6
    // d/dt = (6 - 5) / 0.01 = 100
    // Output = -Kd * d/dt = -1 * 100 = -100
    float output2 = PID_Compute(&test_pid, 10.0f, 6.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -100.0f, output2);
}

void test_PID_Compute_DerivativeOnMeasurement_NotOnError(void)
{
    PID_Init(&test_pid, 0.0f, 0.0f, 1.0f, -100.0f, 100.0f);

    // Setpoint changes from 10 to 20, but measurement stays at 5
    PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    float output = PID_Compute(&test_pid, 20.0f, 5.0f, 0.01f);

    // If derivative is on measurement only, output should be 0 (measurement didn't change)
    // If derivative is on error, output would be non-zero (setpoint changed)
    // The actual implementation uses derivative on measurement
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, output);
}

// =============================================================================
// TEST: Output Limiting
// =============================================================================

void test_PID_Compute_OutputLimiting_ShouldClamp(void)
{
    PID_Init(&test_pid, 10.0f, 0.0f, 0.0f, -5.0f, 5.0f);

    // Error = 10, Kp = 10, raw output = 100
    // Should clamp to 5
    float output = PID_Compute(&test_pid, 10.0f, 0.0f, 0.01f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, output);
}

void test_PID_Compute_OutputLimiting_LowerBound(void)
{
    PID_Init(&test_pid, 10.0f, 0.0f, 0.0f, -5.0f, 5.0f);

    // Error = -10, Kp = 10, raw output = -100
    // Should clamp to -5
    float output = PID_Compute(&test_pid, 0.0f, 10.0f, 0.01f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -5.0f, output);
}

// =============================================================================
// TEST: Setpoint Weighting
// =============================================================================

void test_PID_SetSetpointWeight_ShouldUpdateWeight(void)
{
    PID_Init(&test_pid, 1.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    PID_SetSetpointWeight(&test_pid, 0.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, test_pid.setpoint_weight);
}

void test_PID_SetSetpointWeight_NullPointer_ShouldNotCrash(void)
{
    PID_SetSetpointWeight(NULL, 0.5f);
    TEST_ASSERT_TRUE(1); // Should not crash
}

// =============================================================================
// TEST: Derivative Filtering
// =============================================================================

void test_PID_SetDerivativeFilter_ShouldUpdateFilter(void)
{
    PID_Init(&test_pid, 0.0f, 0.0f, 1.0f, -100.0f, 100.0f);

    PID_SetDerivativeFilter(&test_pid, 0.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, test_pid.derivative_filter);
}

void test_PID_SetDerivativeFilter_NullPointer_ShouldNotCrash(void)
{
    PID_SetDerivativeFilter(NULL, 0.5f);
    TEST_ASSERT_TRUE(1);
}

// =============================================================================
// TEST: Gain Setting
// =============================================================================

void test_PID_SetGains_ShouldUpdateGains(void)
{
    PID_Init(&test_pid, 1.0f, 2.0f, 3.0f, 0.0f, 100.0f);

    PID_SetGains(&test_pid, 4.0f, 5.0f, 6.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, test_pid.Kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, test_pid.Ki);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, test_pid.Kd);
}

void test_PID_SetGains_NullPointer_ShouldNotCrash(void)
{
    PID_SetGains(NULL, 1.0f, 2.0f, 3.0f);
    TEST_ASSERT_TRUE(1);
}

// =============================================================================
// TEST: Reset
// =============================================================================

void test_PID_Reset_ShouldClearState(void)
{
    PID_Init(&test_pid, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);

    // Run some cycles
    PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);
    PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);

    // Reset
    PID_Reset(&test_pid);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_pid.prev_error);
    TEST_ASSERT_FALSE(test_pid.initialized);
}

void test_PID_Reset_NullPointer_ShouldNotCrash(void)
{
    PID_Reset(NULL);
    TEST_ASSERT_TRUE(1);
}

// =============================================================================
// TEST: Get/Set Integral
// =============================================================================

void test_PID_GetIntegral_ShouldReturnIntegral(void)
{
    PID_Init(&test_pid, 0.0f, 1.0f, 0.0f, 0.0f, 100.0f);

    PID_Compute(&test_pid, 10.0f, 5.0f, 0.01f);

    float integral = PID_GetIntegral(&test_pid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.05f, integral);
}

void test_PID_GetIntegral_NullPointer_ShouldReturnZero(void)
{
    float integral = PID_GetIntegral(NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, integral);
}

void test_PID_SetIntegral_ShouldSetIntegral(void)
{
    PID_Init(&test_pid, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);

    PID_SetIntegral(&test_pid, 0.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, PID_GetIntegral(&test_pid));
}

void test_PID_SetIntegral_NullPointer_ShouldNotCrash(void)
{
    PID_SetIntegral(NULL, 0.5f);
    TEST_ASSERT_TRUE(1);
}

// =============================================================================
// TEST: Small Time Steps
// =============================================================================

void test_PID_Compute_SmallDt_ShouldHandleGracefully(void)
{
    PID_Init(&test_pid, 1.0f, 1.0f, 1.0f, -100.0f, 100.0f);

    // Very small dt
    float output = PID_Compute(&test_pid, 10.0f, 5.0f, 0.0001f);

    // Should not produce extreme values
    TEST_ASSERT_TRUE(output < 100.0f);
    TEST_ASSERT_TRUE(output > -100.0f);
}

void test_PID_Compute_ZeroDt_ShouldHandleGracefully(void)
{
    PID_Init(&test_pid, 1.0f, 1.0f, 1.0f, -100.0f, 100.0f);

    // Zero dt - should handle without crashing
    float output = PID_Compute(&test_pid, 10.0f, 5.0f, 0.0f);

    // Proportional term should still work
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, output); // Kp * error = 1 * 5
}

// =============================================================================
// TEST: Full PID Response
// =============================================================================

void test_PID_Compute_FullPID_ShouldCombineTerms(void)
{
    PID_Init(&test_pid, 2.0f, 1.0f, 0.5f, 0.0f, 100.0f);

    // Error = 10, measurement = 5
    // P = 2 * 10 = 20
    // I = 1 * 10 * 0.01 = 0.1
    // D = 0 (first call)
    float output1 = PID_Compute(&test_pid, 15.0f, 5.0f, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.1f, output1);

    // Measurement changes to 7
    // Error = 8, P = 16
    // I = 0.1 + 1 * 8 * 0.01 = 0.18
    // D = 0.5 * (5 - 7) / 0.01 = -100, clamped or filtered
    float output2 = PID_Compute(&test_pid, 15.0f, 7.0f, 0.01f);
    // Just verify it's reasonable
    TEST_ASSERT_TRUE(output2 > -100.0f && output2 < 100.0f);
}

// =============================================================================
// RUN TESTS
// =============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_PID_Init_ShouldInitializeController);
    RUN_TEST(test_PID_Init_ShouldSetLimits);
    RUN_TEST(test_PID_Init_NullPointer_ShouldNotCrash);

    // Proportional tests
    RUN_TEST(test_PID_Compute_Proportional_ShouldProduceOutput);
    RUN_TEST(test_PID_Compute_ZeroError_ShouldProduceZeroOutput);
    RUN_TEST(test_PID_Compute_NegativeError_ShouldProduceNegativeOutput);
    RUN_TEST(test_PID_Compute_NullPointer_ShouldReturnZero);

    // Integral tests
    RUN_TEST(test_PID_Compute_Integral_ShouldAccumulate);

    // Anti-windup tests
    RUN_TEST(test_PID_Compute_AntiWindup_ShouldLimitIntegral);

    // Derivative tests
    RUN_TEST(test_PID_Compute_Derivative_ShouldRespondToChange);
    RUN_TEST(test_PID_Compute_DerivativeOnMeasurement_NotOnError);

    // Output limiting tests
    RUN_TEST(test_PID_Compute_OutputLimiting_ShouldClamp);
    RUN_TEST(test_PID_Compute_OutputLimiting_LowerBound);

    // Setpoint weighting tests
    RUN_TEST(test_PID_SetSetpointWeight_ShouldUpdateWeight);
    RUN_TEST(test_PID_SetSetpointWeight_NullPointer_ShouldNotCrash);

    // Derivative filter tests
    RUN_TEST(test_PID_SetDerivativeFilter_ShouldUpdateFilter);
    RUN_TEST(test_PID_SetDerivativeFilter_NullPointer_ShouldNotCrash);

    // Gain setting tests
    RUN_TEST(test_PID_SetGains_ShouldUpdateGains);
    RUN_TEST(test_PID_SetGains_NullPointer_ShouldNotCrash);

    // Reset tests
    RUN_TEST(test_PID_Reset_ShouldClearState);
    RUN_TEST(test_PID_Reset_NullPointer_ShouldNotCrash);

    // Integral get/set tests
    RUN_TEST(test_PID_GetIntegral_ShouldReturnIntegral);
    RUN_TEST(test_PID_GetIntegral_NullPointer_ShouldReturnZero);
    RUN_TEST(test_PID_SetIntegral_ShouldSetIntegral);
    RUN_TEST(test_PID_SetIntegral_NullPointer_ShouldNotCrash);

    // Small dt tests
    RUN_TEST(test_PID_Compute_SmallDt_ShouldHandleGracefully);
    RUN_TEST(test_PID_Compute_ZeroDt_ShouldHandleGracefully);

    // Full PID test
    RUN_TEST(test_PID_Compute_FullPID_ShouldCombineTerms);

    return UNITY_END();
}
