/**
 * @file test_thermal_runaway.c
 * @brief Unit tests for Thermal Runaway Protection
 *
 * Security Task: SEC-009 / ADP-005
 * Tests dT/dt > 5°C/s detection with emergency shutdown.
 */

#include <unity.h>
#include <string.h>
#include "../src/safety/thermal_runaway.h"

void setUp(void) {
    ThermalRunaway_Init();
}

void tearDown(void) {
    // Reset after each test
    ThermalRunaway_Reset();
}

void test_thermal_runaway_init(void) {
    TEST_ASSERT_TRUE(ThermalRunaway_Init());
    TEST_ASSERT_EQUAL(THERMAL_RUNAWAY_STATE_NORMAL, ThermalRunaway_GetState());
}

void test_thermal_runaway_normal_operation(void) {
    // Normal temperature readings (no rapid change)
    for (int i = 0; i < 20; i++) {
        ThermalRunaway_Update(25.0f + (i * 0.1f), i * 100);
    }
    
    TEST_ASSERT_EQUAL(THERMAL_RUNAWAY_STATE_NORMAL, ThermalRunaway_GetState());
    TEST_ASSERT_FALSE(ThermalRunaway_ShutdownRequired());
}

void test_thermal_runaway_detects_rapid_rise(void) {
    // Simulate rapid temperature rise: 6°C/s (above 5°C/s threshold)
    float temp = 25.0f;
    uint32_t timestamp = 0;
    
    // Fill history with normal readings first
    for (int i = 0; i < 15; i++) {
        ThermalRunaway_Update(temp, timestamp);
        temp += 0.3f;  // 3°C/s (below threshold)
        timestamp += 100;
    }
    
    // Now exceed threshold with 6°C/s rise
    for (int i = 0; i < 5; i++) {
        temp += 0.6f;  // 6°C/s (above threshold)
        timestamp += 100;
        ThermalRunawayState_t state = ThermalRunaway_Update(temp, timestamp);
        
        if (i >= 2) {  // Need a few samples for detection
            // State should be ALARM or SHUTDOWN
            if (state == THERMAL_RUNAWAY_STATE_SHUTDOWN) {
                break;
            }
        }
    }
    
    TEST_ASSERT_TRUE(
        ThermalRunaway_GetState() == THERMAL_RUNAWAY_STATE_ALARM ||
        ThermalRunaway_GetState() == THERMAL_RUNAWAY_STATE_SHUTDOWN
    );
}

void test_thermal_runaway_triggers_shutdown(void) {
    // Simulate rapid rise that triggers emergency shutdown
    float temp = 25.0f;
    uint32_t timestamp = 0;
    
    // Fill history
    for (int i = 0; i < 15; i++) {
        ThermalRunaway_Update(temp, timestamp);
        timestamp += 100;
    }
    
    // Rapid rise that triggers shutdown
    for (int i = 0; i < 10; i++) {
        temp += 1.0f;  // 10°C/s - well above threshold
        timestamp += 100;
        ThermalRunaway_Update(temp, timestamp);
    }
    
    // Wait for shutdown delay (50ms)
    timestamp += 60;
    ThermalRunaway_Update(temp, timestamp);
    
    TEST_ASSERT_EQUAL(THERMAL_RUNAWAY_STATE_SHUTDOWN, ThermalRunaway_GetState());
    TEST_ASSERT_TRUE(ThermalRunaway_ShutdownRequired());
}

void test_thermal_runaway_reset(void) {
    // Trigger shutdown
    float temp = 25.0f;
    uint32_t timestamp = 0;
    
    for (int i = 0; i < 15; i++) {
        ThermalRunaway_Update(temp, timestamp);
        timestamp += 100;
    }
    
    for (int i = 0; i < 10; i++) {
        temp += 1.0f;
        timestamp += 100;
        ThermalRunaway_Update(temp, timestamp);
    }
    
    timestamp += 60;
    ThermalRunaway_Update(temp, timestamp);
    
    TEST_ASSERT_EQUAL(THERMAL_RUNAWAY_STATE_SHUTDOWN, ThermalRunaway_GetState());
    
    // Reset
    TEST_ASSERT_TRUE(ThermalRunaway_Reset());
    TEST_ASSERT_EQUAL(THERMAL_RUNAWAY_STATE_NORMAL, ThermalRunaway_GetState());
    TEST_ASSERT_FALSE(ThermalRunaway_ShutdownRequired());
}

void test_thermal_runaway_get_stats(void) {
    ThermalRunawayStats_t stats;
    
    // Trigger alarm
    float temp = 25.0f;
    uint32_t timestamp = 0;
    
    for (int i = 0; i < 15; i++) {
        ThermalRunaway_Update(temp, timestamp);
        timestamp += 100;
    }
    
    for (int i = 0; i < 5; i++) {
        temp += 1.0f;
        timestamp += 100;
        ThermalRunaway_Update(temp, timestamp);
    }
    
    ThermalRunaway_GetStats(&stats);
    
    TEST_ASSERT_TRUE(stats.alarm_count > 0);
    TEST_ASSERT_TRUE(stats.current_rate_c_per_s > 0.0f);
}

void test_thermal_runaway_configure(void) {
    ThermalRunawayConfig_t config = {
        .dT_dt_threshold_c_per_s = 3.0f,  // Lower threshold
        .sample_interval_ms = 100,
        .emergency_shutdown_enable = true,
        .alarm_hysteresis_c_per_s = 0.5f
    };
    
    TEST_ASSERT_TRUE(ThermalRunaway_Configure(&config));
    
    const ThermalRunawayConfig_t* current = ThermalRunaway_GetConfig();
    TEST_ASSERT_EQUAL_FLOAT(3.0f, current->dT_dt_threshold_c_per_s);
}

void test_thermal_runaway_invalid_configuration(void) {
    ThermalRunawayConfig_t invalid_config = {
        .dT_dt_threshold_c_per_s = -1.0f,  // Invalid: negative
        .sample_interval_ms = 100,
        .emergency_shutdown_enable = true,
        .alarm_hysteresis_c_per_s = 1.0f
    };
    
    TEST_ASSERT_FALSE(ThermalRunaway_Configure(&invalid_config));
}

void test_thermal_runaway_state_strings(void) {
    TEST_ASSERT_EQUAL_STRING("NORMAL", ThermalRunaway_GetStateString(THERMAL_RUNAWAY_STATE_NORMAL));
    TEST_ASSERT_EQUAL_STRING("ALARM", ThermalRunaway_GetStateString(THERMAL_RUNAWAY_STATE_ALARM));
    TEST_ASSERT_EQUAL_STRING("SHUTDOWN", ThermalRunaway_GetStateString(THERMAL_RUNAWAY_STATE_SHUTDOWN));
    TEST_ASSERT_EQUAL_STRING("ERROR", ThermalRunaway_GetStateString(THERMAL_RUNAWAY_STATE_ERROR));
}

void test_thermal_runaway_false_alarm_recovery(void) {
    // Simulate brief spike that doesn't persist (false alarm)
    float temp = 25.0f;
    uint32_t timestamp = 0;
    
    // Normal readings
    for (int i = 0; i < 10; i++) {
        ThermalRunaway_Update(temp, timestamp);
        timestamp += 100;
    }
    
    // Brief spike
    for (int i = 0; i < 3; i++) {
        temp += 0.7f;  // Above threshold briefly
        timestamp += 100;
        ThermalRunaway_Update(temp, timestamp);
    }
    
    // Rate drops back to normal
    for (int i = 0; i < 10; i++) {
        temp += 0.2f;  // Below threshold
        timestamp += 100;
        ThermalRunaway_Update(temp, timestamp);
    }
    
    // Should recover to NORMAL (not SHUTDOWN)
    TEST_ASSERT_EQUAL(THERMAL_RUNAWAY_STATE_NORMAL, ThermalRunaway_GetState());
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_thermal_runaway_init);
    RUN_TEST(test_thermal_runaway_normal_operation);
    RUN_TEST(test_thermal_runaway_detects_rapid_rise);
    RUN_TEST(test_thermal_runaway_triggers_shutdown);
    RUN_TEST(test_thermal_runaway_reset);
    RUN_TEST(test_thermal_runaway_get_stats);
    RUN_TEST(test_thermal_runaway_configure);
    RUN_TEST(test_thermal_runaway_invalid_configuration);
    RUN_TEST(test_thermal_runaway_state_strings);
    RUN_TEST(test_thermal_runaway_false_alarm_recovery);
    
    return UNITY_END();
}
