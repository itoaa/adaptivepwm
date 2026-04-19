/**
 * @file test_wear_leveling.c
 * @brief Unit tests for Flash Wear Leveling (PWM-ARCH-004)
 * 
 * Tests the complete wear leveling implementation including:
 * - Circular buffer operation
 * - Wear statistics tracking
 * - Sector erase counting
 * - Entry validation
 * - Wrap-around handling
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// Mock HAL for testing
#include "stm32f4xx_hal.h"

// Include the code under test
#include "../src/fault_history.h"

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;
static int test_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (cond) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

// ============================================================================
// Test: Wear Statistics Initialization
// ============================================================================
void test_wear_stats_init(void)
{
    printf("\nTest: Wear Statistics Initialization\n");
    
    FlashWearStats_t stats;
    FaultHistory_GetWearStats(&stats);
    
    TEST_ASSERT(stats.total_writes == 0, "Total writes initialized to 0");
    TEST_ASSERT(stats.sector_erases == 0, "Sector erases initialized to 0");
    TEST_ASSERT(stats.wrap_count == 0, "Wrap count initialized to 0");
    TEST_ASSERT(stats.current_index == 0, "Current index initialized to 0");
    TEST_ASSERT(stats.oldest_index == 0, "Oldest index initialized to 0");
    TEST_ASSERT(stats.average_wear == 0.0f, "Average wear initialized to 0");
    TEST_ASSERT(stats.estimated_life_pct == 100, "Estimated life initialized to 100%");
}

// ============================================================================
// Test: Wear Statistics After Writes
// ============================================================================
void test_wear_stats_after_writes(void)
{
    printf("\nTest: Wear Statistics After Writes\n");
    
    // Clear history first
    FaultHistory_Clear();
    
    // Log some faults
    for (int i = 0; i < 10; i++) {
        FaultHistory_Log(FAULT_TYPE_OVER_VOLTAGE, FAULT_SEVERITY_WARNING, 
                        0x0001, i);
    }
    
    FlashWearStats_t stats;
    FaultHistory_GetWearStats(&stats);
    
    TEST_ASSERT(stats.total_writes == 10, "Total writes incremented correctly");
    TEST_ASSERT(stats.current_index == 10 % FAULT_HISTORY_MAX_ENTRIES, 
                "Current index updated correctly");
    TEST_ASSERT(stats.average_wear > 0.0f, "Average wear calculated");
    TEST_ASSERT(stats.estimated_life_pct == 100, "Life still at 100% after few writes");
}

// ============================================================================
// Test: Circular Buffer Indices
// ============================================================================
void test_circular_buffer_indices(void)
{
    printf("\nTest: Circular Buffer Indices\n");
    
    FaultHistory_Clear();
    
    // Write entries up to buffer size
    for (int i = 0; i < FAULT_HISTORY_MAX_ENTRIES; i++) {
        FaultHistory_Log(FAULT_TYPE_OVER_CURRENT, FAULT_SEVERITY_ERROR, 
                        0x0003, i);
    }
    
    FlashWearStats_t stats;
    FaultHistory_GetWearStats(&stats);
    
    TEST_ASSERT(stats.total_writes == FAULT_HISTORY_MAX_ENTRIES, 
                "All entries written");
    TEST_ASSERT(stats.wrap_count == 0, "No wrap yet at exact capacity");
    TEST_ASSERT(stats.current_index == 0, "Current index back to 0");
    
    // Write one more - should trigger wrap
    FaultHistory_Log(FAULT_TYPE_ADC_FAILURE, FAULT_SEVERITY_CRITICAL, 
                    0x0007, 999);
    
    FaultHistory_GetWearStats(&stats);
    
    TEST_ASSERT(stats.wrap_count == 1, "Wrap count incremented");
    TEST_ASSERT(stats.current_index == 1, "Current index at 1 after wrap");
    TEST_ASSERT(stats.oldest_index == 1, "Oldest index updated after wrap");
}

// ============================================================================
// Test: Wear Leveling Validation
// ============================================================================
void test_wear_leveling_validation(void)
{
    printf("\nTest: Wear Leveling Validation\n");
    
    FaultHistory_Clear();
    
    // Write some entries
    for (int i = 0; i < 5; i++) {
        FaultHistory_Log(FAULT_TYPE_PWM_FAULT, FAULT_SEVERITY_ERROR, 
                        0x0006, i * 100);
    }
    
    uint32_t errors = 0;
    bool valid = FaultHistory_ValidateWearLeveling(&errors);
    
    TEST_ASSERT(valid == true, "Validation passes for consistent state");
    TEST_ASSERT(errors == 0, "No errors reported");
}

// ============================================================================
// Test: Wear Status Strings
// ============================================================================
void test_wear_status_strings(void)
{
    printf("\nTest: Wear Status Strings\n");
    
    const char* status_good = FaultHistory_GetWearStatusString(20.0f);
    const char* status_moderate = FaultHistory_GetWearStatusString(45.0f);
    const char* status_warning = FaultHistory_GetWearStatusString(65.0f);
    const char* status_critical = FaultHistory_GetWearStatusString(85.0f);
    
    TEST_ASSERT(strcmp(status_good, "GOOD") == 0, "Good status returned for low wear");
    TEST_ASSERT(strcmp(status_moderate, "MODERATE") == 0, "Moderate status for medium wear");
    TEST_ASSERT(strcmp(status_warning, "WARNING") == 0, "Warning status for high wear");
    TEST_ASSERT(strcmp(status_critical, "CRITICAL") == 0, "Critical status for extreme wear");
}

// ============================================================================
// Test: Wear Stats Formatting
// ============================================================================
void test_wear_stats_formatting(void)
{
    printf("\nTest: Wear Stats Formatting\n");
    
    FaultHistory_Clear();
    
    // Write some entries
    FaultHistory_Log(FAULT_TYPE_OVER_TEMP, FAULT_SEVERITY_WARNING, 
                    0x0004, 42);
    
    FlashWearStats_t stats;
    FaultHistory_GetWearStats(&stats);
    
    char buffer[1024];
    uint16_t written = FaultHistory_FormatWearStats(&stats, buffer, sizeof(buffer));
    
    TEST_ASSERT(written > 0, "Formatting produces output");
    TEST_ASSERT(written < sizeof(buffer), "Output fits in buffer");
    TEST_ASSERT(strstr(buffer, "Flash Wear Statistics") != NULL, 
                "Header present in formatted output");
    TEST_ASSERT(strstr(buffer, "Total writes") != NULL, 
                "Total writes in output");
    TEST_ASSERT(strstr(buffer, "Sector erases") != NULL, 
                "Sector erases in output");
}

// ============================================================================
// Test: Entry Read After Wrap
// ============================================================================
void test_entry_read_after_wrap(void)
{
    printf("\nTest: Entry Read After Wrap\n");
    
    FaultHistory_Clear();
    
    // Fill buffer completely
    for (int i = 0; i < FAULT_HISTORY_MAX_ENTRIES + 5; i++) {
        FaultHistory_Log(FAULT_TYPE_COMMUNICATION, FAULT_SEVERITY_WARNING, 
                        0x000A, i * 1000);
    }
    
    // Read most recent entry (should be the last one written)
    FaultEntry_t entry;
    bool result = FaultHistory_Read(0, &entry);
    
    TEST_ASSERT(result == true, "Can read entry after wrap");
    TEST_ASSERT(entry.context_data == (FAULT_HISTORY_MAX_ENTRIES + 4) * 1000, 
                "Most recent entry has correct context");
    TEST_ASSERT(entry.fault_type == FAULT_TYPE_COMMUNICATION, 
                "Entry fault type correct");
}

// ============================================================================
// Test: Maintenance Prediction with Wear
// ============================================================================
void test_maintenance_prediction_with_wear(void)
{
    printf("\nTest: Maintenance Prediction with Wear\n");
    
    FaultHistory_Clear();
    
    // Simulate high wear by logging many faults
    for (int i = 0; i < 500; i++) {
        FaultHistory_Log(FAULT_TYPE_OVER_VOLTAGE, FAULT_SEVERITY_WARNING, 
                        0x0001, i);
    }
    
    MaintenancePrediction_t prediction;
    FaultHistory_GetMaintenancePrediction(&prediction);
    
    TEST_ASSERT(prediction.health_score <= 100.0f, "Health score is valid");
    TEST_ASSERT(prediction.health_score >= 0.0f, "Health score non-negative");
    TEST_ASSERT(prediction.degradation_rate >= 0.0f, "Degradation rate non-negative");
    
    // Flash wear should be reflected in primary concern
    TEST_ASSERT(prediction.primary_concern != NULL, "Primary concern set");
}

// ============================================================================
// Test: Statistics Persistence
// ============================================================================
void test_statistics_persistence(void)
{
    printf("\nTest: Statistics Persistence\n");
    
    FaultHistory_Clear();
    
    // Log some faults
    for (int i = 0; i < 20; i++) {
        FaultHistory_Log(FAULT_TYPE_WATCHDOG_TIMEOUT, FAULT_SEVERITY_CRITICAL, 
                        0x0008, i);
    }
    
    // Get stats before deinit
    FlashWearStats_t stats_before;
    FaultHistory_GetWearStats(&stats_before);
    
    // Deinit and re-init (simulates power cycle)
    FaultHistory_Deinit();
    FaultHistory_Init();
    
    // Get stats after re-init
    FlashWearStats_t stats_after;
    FaultHistory_GetWearStats(&stats_after);
    
    TEST_ASSERT(stats_after.total_writes == stats_before.total_writes, 
                "Total writes persisted");
    TEST_ASSERT(stats_after.sector_erases == stats_before.sector_erases, 
                "Sector erases persisted");
    TEST_ASSERT(stats_after.wrap_count == stats_before.wrap_count, 
                "Wrap count persisted");
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main(void)
{
    printf("========================================\n");
    printf("Flash Wear Leveling Tests (PWM-ARCH-004)\n");
    printf("========================================\n");
    
    // Initialize fault history
    if (!FaultHistory_Init()) {
        printf("Failed to initialize fault history\n");
        return 1;
    }
    
    // Run all tests
    test_wear_stats_init();
    test_wear_stats_after_writes();
    test_circular_buffer_indices();
    test_wear_leveling_validation();
    test_wear_status_strings();
    test_wear_stats_formatting();
    test_entry_read_after_wrap();
    test_maintenance_prediction_with_wear();
    test_statistics_persistence();
    
    // Summary
    printf("\n========================================\n");
    printf("Test Summary:\n");
    printf("  Total:  %d\n", test_count);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("========================================\n");
    
    return (tests_failed > 0) ? 1 : 0;
}
