/**
 * @file test_efficiency_calc.c
 * @brief Unit tests for efficiency calculation module (PWM-ARCH-005)
 *
 * Validates the efficiency calculation model against known operating points
 * and compares measurement-based vs physics-based methods.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Mock HAL for testing
#include "stm32f4xx_hal.h"

// Minimal stubs for compilation
#define RIPPLE_BUFFER_SIZE 256

// ADC measurement structure - use the one from hal_adc.h
// (removed duplicate definition)

// Include the module under test
#include "../src/efficiency_calc.c"

// Test result tracking
typedef struct {
    const char* name;
    bool passed;
    char message[256];
} TestResult_t;

#define MAX_TESTS 32
static TestResult_t test_results[MAX_TESTS];
static int test_count = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        snprintf(test_results[test_count].message, 256, "FAIL: %s at line %d: %s", \
                 msg, __LINE__, #cond); \
        test_results[test_count].passed = false; \
        test_results[test_count].name = __func__; \
        test_count++; \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(actual, expected, tolerance, msg) do { \
    float diff = fabsf((actual) - (expected)); \
    if (diff > (tolerance)) { \
        snprintf(test_results[test_count].message, 256, \
                 "FAIL: %s - Expected %.4f, got %.4f (diff %.4f, tolerance %.4f)", \
                 msg, (float)(expected), (float)(actual), diff, (float)(tolerance)); \
        test_results[test_count].passed = false; \
        test_results[test_count].name = __func__; \
        test_count++; \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS() do { \
    test_results[test_count].passed = true; \
    test_results[test_count].name = __func__; \
    snprintf(test_results[test_count].message, 256, "PASS"); \
    test_count++; \
    tests_passed++; \
} while(0)

// =============================================================================
// TEST CASES
// =============================================================================

void test_init_defaults(void)
{
    EfficiencyCalcContext_t ctx;

    bool result = EfficiencyCalc_Init(&ctx, EFF_MODE_MEASUREMENT, TOPOLOGY_BUCK);
    ASSERT_TRUE(result == true, "Init should return true");
    ASSERT_TRUE(ctx.mode == EFF_MODE_MEASUREMENT, "Mode should be MEASUREMENT");
    ASSERT_TRUE(ctx.topology == TOPOLOGY_BUCK, "Topology should be BUCK");
    ASSERT_TRUE(ctx.loss_model.rds_on_high > 0, "RDS_ON should be set");
    ASSERT_TRUE(ctx.loss_model.switching_freq > 0, "Switching freq should be set");

    TEST_PASS();
}

void test_estimate_input_current(void)
{
    // Test: 12V in, 5V out, 2A out, 90% efficiency
    // Pout = 5 * 2 = 10W
    // Pin = 10 / 0.9 = 11.11W
    // Iin = 11.11 / 12 = 0.926A

    float iin = EfficiencyCalc_EstimateInputCurrent(5.0f, 2.0f, 12.0f, 0.90f);
    ASSERT_NEAR(iin, 0.926f, 0.01f, "Input current estimate for buck converter");

    // Edge case: zero voltage
    iin = EfficiencyCalc_EstimateInputCurrent(5.0f, 2.0f, 0.0f, 0.90f);
    ASSERT_TRUE(iin == 0.0f, "Should return 0 for zero input voltage");

    TEST_PASS();
}

void test_measurement_mode_ideal_efficiency(void)
{
    EfficiencyCalcContext_t ctx;
    PowerMeasurement_t pm;
    ADC_Measurement_t meas;

    EfficiencyCalc_Init(&ctx, EFF_MODE_MEASUREMENT, TOPOLOGY_BUCK);

    // Simulate ideal buck converter: 12V -> 5V @ 2A
    // Assume 90% efficiency for estimation
    // Pin = 10W / 0.9 = 11.11W
    // Iin = 11.11W / 12V = 0.926A

    meas.vin = 12.0f;
    meas.vout = 5.0f;
    meas.current = 2.0f;  // Output current
    meas.temperature = 25.0f;

    float eff = EfficiencyCalc_FromMeasurements(&ctx, &meas, &pm);

    // Efficiency should be calculated based on estimated Iin
    // We estimate Iin based on 90% efficiency, so calculated eff should be ~90%
    ASSERT_NEAR(eff, 0.90f, 0.02f, "Efficiency should match estimate");
    ASSERT_TRUE(pm.valid == true, "Power measurement should be valid");
    ASSERT_NEAR(pm.pout, 10.0f, 0.1f, "Output power should be 10W");

    TEST_PASS();
}

void test_validation_reasonable_efficiency(void)
{
    PowerMeasurement_t pm;

    // Valid efficiency
    pm.pin = 100.0f;
    pm.pout = 90.0f;
    ASSERT_TRUE(EfficiencyCalc_IsValid(0.90f, &pm), "90% efficiency is valid");

    // Too low
    ASSERT_TRUE(!EfficiencyCalc_IsValid(0.30f, &pm), "30% efficiency is invalid");

    // Too high (impossible)
    ASSERT_TRUE(!EfficiencyCalc_IsValid(1.01f, &pm), "101% efficiency is invalid");

    // Negative (impossible)
    ASSERT_TRUE(!EfficiencyCalc_IsValid(-0.1f, &pm), "Negative efficiency is invalid");

    // Violates power conservation
    pm.pout = 110.0f;  // More out than in
    ASSERT_TRUE(!EfficiencyCalc_IsValid(0.90f, &pm), "Pout > Pin is invalid");

    TEST_PASS();
}

void test_model_buck_converter(void)
{
    EfficiencyCalcContext_t ctx;
    ADC_Measurement_t meas;
    LossModelParams_t lm;

    // Set up realistic loss model for a good buck converter
    lm.rds_on_high = 0.008f;      // 8 mOhm
    lm.rds_on_low = 0.008f;       // 8 mOhm
    lm.gate_charge = 15.0f;       // 15 nC
    lm.vgate = 12.0f;             // 12V gate drive
    lm.inductor_dcr = 0.030f;       // 30 mOhm
    lm.core_loss_k = 0.0001f;       // Core loss coefficient
    lm.core_loss_alpha = 1.3f;      // Frequency exponent
    lm.core_loss_beta = 2.0f;       // Flux density exponent
    lm.cap_esr = 0.010f;            // 10 mOhm
    lm.switching_freq = 20000.0f;    // 20 kHz

    EfficiencyCalc_Init(&ctx, EFF_MODE_MODEL, TOPOLOGY_BUCK);
    EfficiencyCalc_SetLossModel(&ctx, &lm);

    // Typical 12V -> 5V buck at 2A
    // Duty cycle = Vout/Vin = 5/12 = 0.417
    meas.vin = 12.0f;
    meas.vout = 5.0f;
    meas.current = 2.0f;
    meas.temperature = 25.0f;

    float duty = 0.417f;
    float iripple = 0.5f;  // 0.5A peak-to-peak ripple

    float eff = EfficiencyCalc_FromModel(&ctx, &meas, duty, iripple);

    printf("    Buck converter efficiency: %.2f%%\n", eff * 100.0f);

    // Efficiency should be reasonable for a buck converter (85-98%)
    ASSERT_TRUE(eff >= 0.85f && eff <= 0.99f,
                "Buck converter efficiency should be 85-99%");

    TEST_PASS();
}

void test_model_different_topologies(void)
{
    EfficiencyCalcContext_t ctx;
    ADC_Measurement_t meas;
    LossModelParams_t lm;

    // Use moderate loss model
    lm.rds_on_high = 0.010f;
    lm.rds_on_low = 0.010f;
    lm.gate_charge = 15.0f;
    lm.vgate = 12.0f;
    lm.inductor_dcr = 0.050f;
    lm.core_loss_k = 0.0001f;
    lm.core_loss_alpha = 1.3f;
    lm.core_loss_beta = 2.0f;
    lm.cap_esr = 0.010f;
    lm.switching_freq = 20000.0f;

    meas.vin = 12.0f;
    meas.vout = 5.0f;
    meas.current = 2.0f;

    // Test BUCK
    EfficiencyCalc_Init(&ctx, EFF_MODE_MODEL, TOPOLOGY_BUCK);
    EfficiencyCalc_SetLossModel(&ctx, &lm);
    float eff_buck = EfficiencyCalc_FromModel(&ctx, &meas, 0.417f, 0.5f);

    // Test BOOST
    EfficiencyCalc_Init(&ctx, EFF_MODE_MODEL, TOPOLOGY_BOOST);
    EfficiencyCalc_SetLossModel(&ctx, &lm);
    float eff_boost = EfficiencyCalc_FromModel(
        &ctx, &meas, 0.583f, 0.5f);

    printf("    Buck: %.2f%%, Boost: %.2f%%\n", eff_buck * 100, eff_boost * 100);

    // Both should give reasonable efficiencies
    ASSERT_TRUE(eff_buck > 0.8f && eff_buck <= 0.99f,
                "Buck topology efficiency in range");
    ASSERT_TRUE(eff_boost > 0.8f && eff_boost <= 0.99f,
                "Boost topology efficiency in range");

    TEST_PASS();
}

void test_filtering_moving_average(void)
{
    EfficiencyCalcContext_t ctx;

    EfficiencyCalc_Init(&ctx, EFF_MODE_MEASUREMENT, TOPOLOGY_BUCK);

    // Add several efficiency values
    float filtered = EfficiencyCalc_GetFiltered(&ctx, 0.90f);
    filtered = EfficiencyCalc_GetFiltered(&ctx, 0.91f);
    filtered = EfficiencyCalc_GetFiltered(&ctx, 0.89f);
    filtered = EfficiencyCalc_GetFiltered(&ctx, 0.90f);

    // Filtered value should be close to the average
    ASSERT_NEAR(filtered, 0.90f, 0.02f, "Filtered value should be near average");

    TEST_PASS();
}

void test_loss_breakdown(void)
{
    EfficiencyCalcContext_t ctx;
    ADC_Measurement_t meas;
    float losses[LOSS_COMPONENT_COUNT];

    EfficiencyCalc_Init(&ctx, EFF_MODE_MODEL, TOPOLOGY_BUCK);

    meas.vin = 12.0f;
    meas.vout = 5.0f;
    meas.current = 2.0f;

    float total = EfficiencyCalc_GetLossBreakdown(
        &ctx, &meas, 0.417f, 0.5f, losses, LOSS_COMPONENT_COUNT);

    // Total losses should be positive
    ASSERT_TRUE(total > 0, "Total losses should be positive");

    // Conduction losses should be present
    ASSERT_TRUE(losses[LOSS_CONDUCTION_HS] > 0, "High-side conduction loss should exist");
    ASSERT_TRUE(losses[LOSS_CONDUCTION_LS] > 0, "Low-side conduction loss should exist");

    TEST_PASS();
}

void test_edge_cases(void)
{
    EfficiencyCalcContext_t ctx;
    PowerMeasurement_t pm;
    ADC_Measurement_t meas;

    EfficiencyCalc_Init(&ctx, EFF_MODE_MEASUREMENT, TOPOLOGY_BUCK);

    // Zero voltage
    meas.vin = 0.0f;
    meas.vout = 0.0f;
    meas.current = 0.0f;

    float eff = EfficiencyCalc_FromMeasurements(&ctx, &meas, &pm);
    ASSERT_TRUE(eff == 0.0f, "Zero power should return 0 efficiency");
    ASSERT_TRUE(pm.valid == false, "Zero power should be invalid");

    // Very low power
    meas.vin = 12.0f;
    meas.vout = 5.0f;
    meas.current = 0.001f;  // 1mA

    eff = EfficiencyCalc_FromMeasurements(&ctx, &meas, &pm);
    // Should handle gracefully (low power might be invalid)

    TEST_PASS();
}

void test_comparison_old_vs_new_model(void)
{
    // Compare the old simplified formula with the new validated model

    EfficiencyCalcContext_t ctx;
    ADC_Measurement_t meas;
    LossModelParams_t lm;

    // Setup typical conditions
    meas.vin = 12.0f;
    meas.vout = 5.0f;
    meas.current = 2.0f;

    float duty = 0.417f;
    float iripple = 0.5f;
    float inductance_mH = 100.0f;  // 100uH = 0.1mH
    float esr_mOhm = 50.0f;

    // OLD MODEL (simplified - what was in freertos_tasks.c):
    // switching_loss = 0.01 * L_mH * D^2
    // conduction_loss = ESR_mOhm/1000 * I_ripple^2
    // efficiency = 1 - (switching_loss + conduction_loss)

    float old_switching = 0.01f * inductance_mH * duty * duty;
    float old_conduction = esr_mOhm / 1000.0f * iripple * iripple;
    float old_efficiency = 1.0f - (old_switching + old_conduction);

    // NEW MODEL (physics-based) - with moderate losses
    lm.rds_on_high = 0.015f;
    lm.rds_on_low = 0.015f;
    lm.gate_charge = 20.0f;
    lm.vgate = 12.0f;
    lm.inductor_dcr = 0.080f;
    lm.core_loss_k = 0.0001f;
    lm.core_loss_alpha = 1.3f;
    lm.core_loss_beta = 2.0f;
    lm.cap_esr = 0.020f;
    lm.switching_freq = 20000.0f;

    EfficiencyCalc_Init(&ctx, EFF_MODE_MODEL, TOPOLOGY_BUCK);
    EfficiencyCalc_SetLossModel(&ctx, &lm);
    float new_efficiency = EfficiencyCalc_FromModel(&ctx, &meas, duty, iripple);

    printf("    Old model efficiency: %.4f (%.2f%%)\n", old_efficiency, old_efficiency*100);
    printf("    New model efficiency: %.4f (%.2f%%)\n", new_efficiency, new_efficiency*100);

    // The new model should give a more physically accurate result
    // Old model might give impossible values
    ASSERT_TRUE(new_efficiency >= EFF_MIN_REASONABLE &&
                new_efficiency <= EFF_MAX_REASONABLE,
                "New model gives physically reasonable efficiency");

    // Old model might exceed 100% or be negative with certain parameters
    printf("    Old model valid: %s\n",
           (old_efficiency >= 0.5f && old_efficiency <= 0.99f) ? "YES" : "NO");

    TEST_PASS();
}

// =============================================================================
// TEST RUNNER
// =============================================================================

int main(void)
{
    printf("========================================\n");
    printf("Efficiency Calculation Tests (PWM-ARCH-005)\n");
    printf("========================================\n\n");

    // Run all tests
    test_init_defaults();
    test_estimate_input_current();
    test_measurement_mode_ideal_efficiency();
    test_validation_reasonable_efficiency();
    test_model_buck_converter();
    test_model_different_topologies();
    test_filtering_moving_average();
    test_loss_breakdown();
    test_edge_cases();
    test_comparison_old_vs_new_model();

    // Print results
    printf("\n========================================\n");
    printf("TEST RESULTS\n");
    printf("========================================\n");

    for (int i = 0; i < test_count; i++) {
        printf("[%s] %s\n",
               test_results[i].passed ? "PASS" : "FAIL",
               test_results[i].name);
        if (!test_results[i].passed) {
            printf("       %s\n", test_results[i].message);
        }
    }

    printf("\n----------------------------------------\n");
    printf("Total: %d tests, %d passed, %d failed\n",
           test_count, tests_passed, tests_failed);
    printf("========================================\n");

    return (tests_failed > 0) ? 1 : 0;
}