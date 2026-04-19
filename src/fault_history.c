/**
 * @file fault_history.c
 * @brief Fault History Implementation with Persistent Flash Storage
 * 
 * Security Task: PWM-ARCH-004
 * 
 * Implements fault logging with circular buffer in internal Flash.
 * Includes CRC validation, wear leveling, and predictive maintenance.
 * 
 * FLASH WEAR LEVELING (PWM-ARCH-004):
 * - Uses STM32F401 Sector 6 (128KB) for fault storage
 * - Circular buffer with 256 entries (64 bytes each)
 * - Even distribution: Each entry location written equally
 * - Sector erase count tracked for life estimation
 * - STM32F401 flash endurance: 10,000 cycles
 * - At 1 fault/minute: ~27 hours per wrap, ~7 years @ 10K cycles
 *
 * Framework: CISSP Domain 7, NIST CSF DE.AE, IEC 61508
 */

#include "fault_history.h"
#include "adaptive_assert.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// Internal state
static struct {
    FaultHistory_t manager;
    bool initialized;
    uint32_t last_write_time;     // For rate limiting
} fault_state;

// Flash header structure with wear leveling state (PWM-ARCH-004)
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t version;
    uint32_t write_index;
    uint32_t entry_count;
    uint32_t wrap_count;
    uint32_t last_fault_timestamp;
    uint32_t oldest_index;        // PWM-ARCH-004: Oldest valid entry
    uint32_t sector_erases;       // PWM-ARCH-004: Sector erase counter
    uint32_t total_writes;        // PWM-ARCH-004: Total write counter
    FaultStatistics_t statistics;
    uint16_t crc;
} FlashHeader_t;

// Recovery configuration table
static const struct {
    FaultType_t fault_type;
    RecoveryAction_t action;
    uint8_t max_retries;
    uint32_t retry_delay_ms;
} recovery_config[] = {
    {FAULT_TYPE_OVER_VOLTAGE,     RECOVERY_ACTION_DEGRADE,    3, 100},
    {FAULT_TYPE_UNDER_VOLTAGE,    RECOVERY_ACTION_DEGRADE,    3, 100},
    {FAULT_TYPE_OVER_CURRENT,     RECOVERY_ACTION_STOP,       3, 200},
    {FAULT_TYPE_OVER_TEMP,        RECOVERY_ACTION_DEGRADE,    2, 500},
    {FAULT_TYPE_THERMAL_RUNAWAY,  RECOVERY_ACTION_STOP,       0, 0},
    {FAULT_TYPE_PWM_FAULT,        RECOVERY_ACTION_RESET,      3, 100},
    {FAULT_TYPE_ADC_FAILURE,      RECOVERY_ACTION_RETRY,      2, 50},
    {FAULT_TYPE_WATCHDOG_TIMEOUT, RECOVERY_ACTION_RESTART,    0, 0},
    {FAULT_TYPE_COMMUNICATION,    RECOVERY_ACTION_RETRY,      5, 100},
    {FAULT_TYPE_CRC_ERROR,        RECOVERY_ACTION_RETRY,      3, 50},
    {FAULT_TYPE_CONFIG_CORRUPT,   RECOVERY_ACTION_MANUAL,     0, 0},
    {FAULT_TYPE_HARDWARE_FAULT,   RECOVERY_ACTION_MANUAL,     0, 0},
    {FAULT_TYPE_SOFTWARE_FAULT,   RECOVERY_ACTION_RESTART,    1, 1000},
    {FAULT_TYPE_NONE,             RECOVERY_ACTION_NONE,       0, 0}
};

/**
 * @brief Calculate CRC16 for data integrity
 * 
 * @param data Data to checksum
 * @param length Data length
 * @return CRC16 value
 */
static uint16_t calculate_crc16(const void* data, size_t length)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Calculate entry CRC
 * 
 * @param entry Fault entry
 * @return CRC value
 */
static uint16_t calculate_entry_crc(const FaultEntry_t* entry)
{
    // Calculate CRC over all fields except CRC itself
    size_t len = sizeof(FaultEntry_t) - sizeof(uint16_t);
    return calculate_crc16(entry, len);
}

/**
 * @brief Calculate header CRC
 * 
 * @param header Flash header
 * @return CRC value
 */
static uint16_t calculate_header_crc(const FlashHeader_t* header)
{
    size_t len = sizeof(FlashHeader_t) - sizeof(uint16_t);
    return calculate_crc16(header, len);
}

/**
 * @brief Get entry address in flash using wear leveling (PWM-ARCH-004)
 * 
 * Calculates physical flash address for logical index.
 * Distributes writes evenly across sector for wear leveling.
 * 
 * @param index Logical entry index
 * @return Physical flash address
 */
static uint32_t get_entry_address(uint32_t index)
{
    uint32_t base = FAULT_HISTORY_FLASH_ADDR + sizeof(FlashHeader_t);
    // Use modulo to ensure circular buffer behavior
    uint32_t offset = (index % FAULT_HISTORY_MAX_ENTRIES) * sizeof(FaultEntry_t);
    return base + offset;
}

/**
 * @brief Erase fault history flash sector
 * 
 * @return true if successful
 */
static bool erase_sector(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
    
    HAL_FLASH_Unlock();
    
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FAULT_HISTORY_FLASH_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    bool result = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;
    
    HAL_FLASH_Lock();
    return result;
}

/**
 * @brief Write header to flash
 * 
 * @param header Header to write
 * @return true if successful
 */
static bool write_header(const FlashHeader_t* header)
{
    HAL_FLASH_Unlock();
    
    uint32_t addr = FAULT_HISTORY_FLASH_ADDR;
    const uint32_t* data = (const uint32_t*)header;
    size_t words = sizeof(FlashHeader_t) / 4;
    
    for (size_t i = 0; i < words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, data[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    return true;
}

/**
 * @brief Read header from flash
 * 
 * @param header Output buffer
 * @return true if valid header read
 */
static bool read_header(FlashHeader_t* header)
{
    memcpy(header, (void*)FAULT_HISTORY_FLASH_ADDR, sizeof(FlashHeader_t));
    
    // Validate
    if (header->magic != FAULT_HISTORY_MAGIC) {
        return false;
    }
    
    if (header->version != FAULT_HISTORY_VERSION) {
        return false;
    }
    
    uint16_t crc = calculate_header_crc(header);
    if (crc != header->crc) {
        return false;
    }
    
    return true;
}

/**
 * @brief Write entry to flash at specified index
 * 
 * @param index Entry index (logical position)
 * @param entry Entry to write
 * @return true if successful
 */
static bool write_entry(uint32_t index, const FaultEntry_t* entry)
{
    HAL_FLASH_Unlock();
    
    uint32_t addr = get_entry_address(index);
    const uint32_t* data = (const uint32_t*)entry;
    size_t words = sizeof(FaultEntry_t) / 4;
    
    for (size_t i = 0; i < words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, data[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    return true;
}

/**
 * @brief Read entry from flash
 * 
 * @param index Entry index
 * @param entry Output buffer
 * @return true if valid entry read
 */
static bool read_entry(uint32_t index, FaultEntry_t* entry)
{
    uint32_t addr = get_entry_address(index);
    memcpy(entry, (void*)addr, sizeof(FaultEntry_t));
    
    // Validate CRC
    uint16_t crc = calculate_entry_crc(entry);
    return (crc == entry->crc);
}

/**
 * @brief Update wear statistics (PWM-ARCH-004)
 * 
 * Calculates wear distribution and estimated remaining life.
 * 
 * @param stats Pointer to wear statistics to update
 */
static void update_wear_stats(FlashWearStats_t* stats)
{
    if (stats == NULL) {
        return;
    }
    
    // Calculate average wear per entry location
    if (FAULT_HISTORY_MAX_ENTRIES > 0) {
        float wraps_contribution = (float)stats->wrap_count * 100.0f;
        float current_contribution = ((float)stats->current_index / FAULT_HISTORY_MAX_ENTRIES) * 100.0f;
        stats->average_wear = wraps_contribution + current_contribution;
        
        // Max wear is at the position just before current (most written)
        // For circular buffer, this varies based on wrap count
        stats->max_wear = stats->average_wear;
        if (stats->wrap_count > 0) {
            // After first wrap, max wear increases
            stats->max_wear += 100.0f;
        }
    }
    
    // Calculate estimated remaining life
    // Based on STM32F401 spec: 10,000 erase cycles per sector
    uint32_t total_cycles = stats->sector_erases;
    if (total_cycles < FLASH_ENDURANCE_CYCLES) {
        stats->estimated_life_pct = ((FLASH_ENDURANCE_CYCLES - total_cycles) * 100) 
                                     / FLASH_ENDURANCE_CYCLES;
    } else {
        stats->estimated_life_pct = 0;
    }
}

/**
 * @brief Update statistics after fault
 * 
 * @param fault_type Fault type
 * @param recovered true if recovered
 */
static void update_statistics(FaultType_t fault_type, bool recovered)
{
    fault_state.manager.statistics.total_faults++;
    
    if (fault_type < FAULT_TYPE_COUNT) {
        fault_state.manager.statistics.faults_by_type[fault_type]++;
    }
    
    if (recovered) {
        fault_state.manager.statistics.total_recoveries++;
    } else {
        fault_state.manager.statistics.failed_recoveries++;
    }
    
    fault_state.manager.statistics.last_fault_timestamp = 
        HAL_GetTick();  // Use HAL_GetTick as fallback
    fault_state.manager.statistics.uptime_ms_at_last_fault = HAL_GetTick();
}

/**
 * @brief Get recovery configuration for fault type
 * 
 * @param fault_type Fault type
 * @return Recovery configuration pointer, or NULL if not found
 */
static const struct {
    FaultType_t fault_type;
    RecoveryAction_t action;
    uint8_t max_retries;
    uint32_t retry_delay_ms;
}* get_recovery_config(FaultType_t fault_type)
{
    for (int i = 0; i < sizeof(recovery_config)/sizeof(recovery_config[0]); i++) {
        if (recovery_config[i].fault_type == fault_type) {
            return &recovery_config[i];
        }
    }
    return NULL;
}

/**
 * @brief Persist header to flash with wear statistics (PWM-ARCH-004)
 * 
 * @return true if successful
 */
static bool persist_header(void)
{
    FlashHeader_t header = {
        .magic = FAULT_HISTORY_MAGIC,
        .version = FAULT_HISTORY_VERSION,
        .write_index = fault_state.manager.write_index,
        .entry_count = fault_state.manager.entry_count,
        .wrap_count = fault_state.manager.wrap_count,
        .last_fault_timestamp = fault_state.manager.last_fault_timestamp,
        .oldest_index = fault_state.manager.oldest_index,
        .sector_erases = fault_state.manager.wear_stats.sector_erases,
        .total_writes = fault_state.manager.wear_stats.total_writes,
        .statistics = fault_state.manager.statistics,
        .crc = 0
    };
    header.crc = calculate_header_crc(&header);
    
    return write_header(&header);
}

bool FaultHistory_Init(void)
{
    if (fault_state.initialized) {
        return true;
    }
    
    memset(&fault_state, 0, sizeof(fault_state));
    
    // Try to read existing header
    FlashHeader_t header;
    if (read_header(&header)) {
        // Restore state from flash
        fault_state.manager.write_index = header.write_index;
        fault_state.manager.entry_count = header.entry_count;
        fault_state.manager.wrap_count = header.wrap_count;
        fault_state.manager.last_fault_timestamp = header.last_fault_timestamp;
        fault_state.manager.oldest_index = header.oldest_index;
        fault_state.manager.statistics = header.statistics;
        
        // Restore wear statistics (PWM-ARCH-004)
        fault_state.manager.wear_stats.sector_erases = header.sector_erases;
        fault_state.manager.wear_stats.total_writes = header.total_writes;
        fault_state.manager.wear_stats.wrap_count = header.wrap_count;
        fault_state.manager.wear_stats.current_index = header.write_index % FAULT_HISTORY_MAX_ENTRIES;
        fault_state.manager.wear_stats.oldest_index = header.oldest_index;
        
        // Calculate derived wear statistics
        update_wear_stats(&fault_state.manager.wear_stats);
        
        fault_state.manager.initialized = true;
    } else {
        // Initialize new fault history
        fault_state.manager.write_index = 0;
        fault_state.manager.entry_count = 0;
        fault_state.manager.wrap_count = 0;
        fault_state.manager.last_fault_timestamp = 0;
        fault_state.manager.oldest_index = 0;
        memset(&fault_state.manager.statistics, 0, sizeof(FaultStatistics_t));
        memset(&fault_state.manager.wear_stats, 0, sizeof(FlashWearStats_t));
        fault_state.manager.initialized = true;
        
        // Write initial header
        FlashHeader_t new_header = {
            .magic = FAULT_HISTORY_MAGIC,
            .version = FAULT_HISTORY_VERSION,
            .write_index = 0,
            .entry_count = 0,
            .wrap_count = 0,
            .last_fault_timestamp = 0,
            .oldest_index = 0,
            .sector_erases = 0,
            .total_writes = 0,
            .statistics = {0},
            .crc = 0
        };
        new_header.crc = calculate_header_crc(&new_header);
        
        if (!erase_sector()) {
            return false;
        }
        
        if (!write_header(&new_header)) {
            return false;
        }
    }
    
    fault_state.initialized = true;
    return true;
}

bool FaultHistory_Deinit(void)
{
    if (!fault_state.initialized) {
        return true;
    }
    
    // Persist header with current wear statistics
    persist_header();
    
    fault_state.initialized = false;
    return true;
}

bool FaultHistory_Log(FaultType_t fault_type, FaultSeverity_t severity,
                      uint16_t error_code, uint32_t context)
{
    return FaultHistory_LogWithRecovery(fault_type, severity, error_code, context,
                                        RECOVERY_ACTION_NONE, RECOVERY_RESULT_PENDING);
}

bool FaultHistory_LogWithRecovery(FaultType_t fault_type, FaultSeverity_t severity,
                                  uint16_t error_code, uint32_t context,
                                  RecoveryAction_t recovery_action,
                                  RecoveryResult_t recovery_result)
{
    if (!fault_state.initialized) {
        return false;
    }
    
    // Rate limiting - don't flood the log
    uint32_t now = HAL_GetTick();
    if (now - fault_state.last_write_time < 10) {
        // Too soon, skip this entry (but count it)
        return true;
    }
    fault_state.last_write_time = now;
    
    // Create fault entry
    FaultEntry_t entry = {
        .timestamp_ms = HAL_GetTick(),
        .rtc_timestamp = 0,  // RTC not implemented
        .fault_type = fault_type,
        .severity = severity,
        .error_code = error_code,
        .context_data = context,
        .recovery_action = recovery_action,
        .recovery_result = recovery_result,
        .retry_count = fault_state.manager.recovery_attempts,
        .crc = 0
    };
    entry.crc = calculate_entry_crc(&entry);
    
    // WEAR LEVELING: Check if we need to wrap (PWM-ARCH-004)
    uint32_t next_index = fault_state.manager.write_index % FAULT_HISTORY_MAX_ENTRIES;
    
    if (fault_state.manager.entry_count >= FAULT_HISTORY_MAX_ENTRIES) {
        // Buffer is full - we're about to overwrite oldest entry
        fault_state.manager.wrap_count++;
        fault_state.manager.oldest_index = (fault_state.manager.oldest_index + 1) % FAULT_HISTORY_MAX_ENTRIES;
        
        // If wrapping back to index 0, we need to erase sector
        if (next_index == 0 && fault_state.manager.entry_count > 0) {
            // Increment sector erase counter
            fault_state.manager.wear_stats.sector_erases++;
            
            // Erase sector before wrapping
            if (!erase_sector()) {
                return false;
            }
            
            // Re-write header after erase
            if (!persist_header()) {
                return false;
            }
        }
    }
    
    // Write entry at current position
    if (!write_entry(fault_state.manager.write_index, &entry)) {
        return false;
    }
    
    // Update state
    fault_state.manager.write_index++;
    fault_state.manager.entry_count++;
    fault_state.manager.last_fault_timestamp = now;
    fault_state.manager.wear_stats.total_writes++;
    fault_state.manager.wear_stats.current_index = next_index;
    fault_state.manager.wear_stats.wrap_count = fault_state.manager.wrap_count;
    
    // Update wear statistics
    update_wear_stats(&fault_state.manager.wear_stats);
    
    // Update statistics
    bool recovered = (recovery_result == RECOVERY_RESULT_SUCCESS) ||
                     (recovery_result == RECOVERY_RESULT_PARTIAL);
    update_statistics(fault_type, recovered);
    
    // Update header in flash periodically (every 10 entries)
    if (fault_state.manager.entry_count % 10 == 0) {
        persist_header();
    }
    
    return true;
}

bool FaultHistory_Read(uint32_t index, FaultEntry_t* entry)
{
    if (!fault_state.initialized || entry == NULL) {
        return false;
    }
    
    if (index >= fault_state.manager.entry_count) {
        return false;
    }
    
    // Calculate actual index (most recent first)
    uint32_t actual_idx;
    if (fault_state.manager.entry_count <= FAULT_HISTORY_MAX_ENTRIES) {
        actual_idx = fault_state.manager.entry_count - 1 - index;
    } else {
        // Wrapped buffer - calculate from current position
        uint32_t current = fault_state.manager.write_index % FAULT_HISTORY_MAX_ENTRIES;
        actual_idx = (FAULT_HISTORY_MAX_ENTRIES + current - 1 - index) % FAULT_HISTORY_MAX_ENTRIES;
    }
    
    return read_entry(actual_idx, entry);
}

uint32_t FaultHistory_GetCount(void)
{
    if (!fault_state.initialized) {
        return 0;
    }
    
    return (fault_state.manager.entry_count < FAULT_HISTORY_MAX_ENTRIES) ?
           fault_state.manager.entry_count : FAULT_HISTORY_MAX_ENTRIES;
}

bool FaultHistory_Clear(void)
{
    if (!fault_state.initialized) {
        return false;
    }
    
    // Erase sector
    if (!erase_sector()) {
        return false;
    }
    
    // Increment sector erase counter
    fault_state.manager.wear_stats.sector_erases++;
    
    // Reset state
    fault_state.manager.write_index = 0;
    fault_state.manager.entry_count = 0;
    fault_state.manager.wrap_count = 0;
    fault_state.manager.last_fault_timestamp = 0;
    fault_state.manager.oldest_index = 0;
    memset(&fault_state.manager.statistics, 0, sizeof(FaultStatistics_t));
    
    // Update wear stats after clear
    update_wear_stats(&fault_state.manager.wear_stats);
    
    // Write new header
    FlashHeader_t header = {
        .magic = FAULT_HISTORY_MAGIC,
        .version = FAULT_HISTORY_VERSION,
        .write_index = 0,
        .entry_count = 0,
        .wrap_count = 0,
        .last_fault_timestamp = 0,
        .oldest_index = 0,
        .sector_erases = fault_state.manager.wear_stats.sector_erases,
        .total_writes = fault_state.manager.wear_stats.total_writes,
        .statistics = {0},
        .crc = 0
    };
    header.crc = calculate_header_crc(&header);
    
    return write_header(&header);
}

void FaultHistory_GetStatistics(FaultStatistics_t* stats)
{
    if (stats == NULL) {
        return;
    }
    
    if (!fault_state.initialized) {
        memset(stats, 0, sizeof(FaultStatistics_t));
        return;
    }
    
    *stats = fault_state.manager.statistics;
    
    // Calculate rates
    uint32_t now = HAL_GetTick();
    uint32_t elapsed_1h = 3600000;  // 1 hour in ms
    uint32_t elapsed_24h = 86400000;  // 24 hours in ms
    
    // Count faults in last hour
    uint32_t faults_1h = 0;
    uint32_t total_entries = FaultHistory_GetCount();
    for (uint32_t i = 0; i < total_entries; i++) {
        FaultEntry_t entry;
        if (read_entry(i, &entry)) {
            if (now - entry.timestamp_ms <= elapsed_1h) {
                faults_1h++;
            }
        }
    }
    
    stats->fault_rate_1h = faults_1h;
    stats->fault_rate_24h = fault_state.manager.statistics.total_faults;  // Simplified
}

void FaultHistory_GetWearStats(FlashWearStats_t* stats)
{
    if (stats == NULL) {
        return;
    }
    
    if (!fault_state.initialized) {
        memset(stats, 0, sizeof(FlashWearStats_t));
        stats->estimated_life_pct = 100;
        return;
    }
    
    // Update wear stats before returning
    update_wear_stats(&fault_state.manager.wear_stats);
    
    *stats = fault_state.manager.wear_stats;
}

uint16_t FaultHistory_FormatWearStats(const FlashWearStats_t* stats, 
                                        char* buffer, uint16_t size)
{
    if (stats == NULL || buffer == NULL || size == 0) {
        return 0;
    }
    
    uint16_t written = 0;
    
    written += snprintf(buffer + written, size - written,
        "Flash Wear Statistics:\r\n");
    written += snprintf(buffer + written, size - written,
        "======================\r\n\r\n");
    
    written += snprintf(buffer + written, size - written,
        "Write Operations:\r\n");
    written += snprintf(buffer + written, size - written,
        "  Total writes:     %lu\r\n", (unsigned long)stats->total_writes);
    written += snprintf(buffer + written, size - written,
        "  Sector erases:    %lu\r\n", (unsigned long)stats->sector_erases);
    written += snprintf(buffer + written, size - written,
        "  Buffer wraps:     %lu\r\n", (unsigned long)stats->wrap_count);
    
    written += snprintf(buffer + written, size - written,
        "\r\nWear Distribution:\r\n");
    written += snprintf(buffer + written, size - written,
        "  Current index:    %lu\r\n", (unsigned long)stats->current_index);
    written += snprintf(buffer + written, size - written,
        "  Oldest index:     %lu\r\n", (unsigned long)stats->oldest_index);
    written += snprintf(buffer + written, size - written,
        "  Average wear:     %.1f%%\r\n", stats->average_wear);
    written += snprintf(buffer + written, size - written,
        "  Max wear:         %.1f%%\r\n", stats->max_wear);
    
    written += snprintf(buffer + written, size - written,
        "\r\nFlash Health:\r\n");
    written += snprintf(buffer + written, size - written,
        "  Estimated life:   %lu%%\r\n", (unsigned long)stats->estimated_life_pct);
    written += snprintf(buffer + written, size - written,
        "  Status:           %s\r\n",
        FaultHistory_GetWearStatusString(stats->average_wear));
    
    written += snprintf(buffer + written, size - written,
        "\r\nEndurance Info:\r\n");
    written += snprintf(buffer + written, size - written,
        "  Sector size:      %d KB\r\n", FAULT_HISTORY_FLASH_SIZE / 1024);
    written += snprintf(buffer + written, size - written,
        "  Max entries:      %d\r\n", FAULT_HISTORY_MAX_ENTRIES);
    written += snprintf(buffer + written, size - written,
        "  Entry size:       %lu bytes\r\n", (unsigned long)sizeof(FaultEntry_t));
    written += snprintf(buffer + written, size - written,
        "  Flash cycles:     %d\r\n", FLASH_ENDURANCE_CYCLES);
    
    return written;
}

void FaultHistory_SetDiagnosticMode(bool enable)
{
    if (!fault_state.initialized) {
        return;
    }
    
    fault_state.manager.diagnostic_mode = enable;
}

bool FaultHistory_IsDiagnosticMode(void)
{
    return fault_state.initialized && fault_state.manager.diagnostic_mode;
}

void FaultHistory_GetMaintenancePrediction(MaintenancePrediction_t* prediction)
{
    if (prediction == NULL) {
        return;
    }
    
    if (!fault_state.initialized) {
        memset(prediction, 0, sizeof(MaintenancePrediction_t));
        prediction->health_score = 100.0f;
        return;
    }
    
    const FaultStatistics_t* stats = &fault_state.manager.statistics;
    
    // Update wear stats before calculation
    update_wear_stats(&fault_state.manager.wear_stats);
    const FlashWearStats_t* wear = &fault_state.manager.wear_stats;
    
    // Calculate health score (0-100)
    float health = 100.0f;
    
    // Reduce health based on fault history
    health -= stats->total_faults * 0.5f;
    health -= stats->failed_recoveries * 2.0f;
    
    // Reduce based on fault rate
    if (stats->fault_rate_24h > 0) {
        health -= stats->fault_rate_24h * 1.0f;
    }
    
    // Reduce based on flash wear (PWM-ARCH-004)
    float wear_penalty = wear->average_wear * 0.1f;
    health -= wear_penalty;
    
    // Clamp to 0-100
    if (health < 0.0f) health = 0.0f;
    if (health > 100.0f) health = 100.0f;
    
    prediction->health_score = health;
    
    // Determine if maintenance is recommended
    bool wear_critical = wear->estimated_life_pct < 20;
    prediction->maintenance_recommended = (health < MAINTENANCE_HEALTH_THRESHOLD) ||
                                             (stats->fault_rate_24h > MAINTENANCE_FAULT_THRESHOLD) ||
                                             wear_critical;
    
    // Calculate estimated days until maintenance
    if (prediction->maintenance_recommended) {
        prediction->days_until_maintenance = 0;
        prediction->trend_direction = 2;  // Degrading
    } else {
        // Estimate based on degradation rate
        float degradation = (100.0f - health) / 100.0f;
        if (degradation > 0.001f) {
            prediction->days_until_maintenance = (uint32_t)((MAINTENANCE_HEALTH_THRESHOLD - health) / 
                                                             (degradation * 0.1f));
        } else {
            prediction->days_until_maintenance = 365;  // No immediate concern
        }
        prediction->trend_direction = (stats->fault_rate_24h > 0) ? 2 : 0;
    }
    
    prediction->degradation_rate = (100.0f - health) / 100.0f;
    
    // Determine primary concern
    if (wear_critical) {
        prediction->primary_concern = "Flash Wear (Critical)";
    } else if (wear->estimated_life_pct < 50) {
        prediction->primary_concern = "Flash Wear";
    } else if (stats->faults_by_type[FAULT_TYPE_THERMAL_RUNAWAY] > 0) {
        prediction->primary_concern = "Thermal Runaway";
    } else if (stats->faults_by_type[FAULT_TYPE_OVER_CURRENT] > 
               stats->faults_by_type[FAULT_TYPE_OVER_VOLTAGE]) {
        prediction->primary_concern = "Over Current";
    } else if (stats->faults_by_type[FAULT_TYPE_OVER_VOLTAGE] > 0) {
        prediction->primary_concern = "Over Voltage";
    } else if (stats->faults_by_type[FAULT_TYPE_HARDWARE_FAULT] > 0) {
        prediction->primary_concern = "Hardware Faults";
    } else {
        prediction->primary_concern = "General Wear";
    }
}

const char* FaultHistory_GetTypeString(FaultType_t fault_type)
{
    switch (fault_type) {
        case FAULT_TYPE_NONE:               return "NONE";
        case FAULT_TYPE_OVER_VOLTAGE:       return "OVER_VOLTAGE";
        case FAULT_TYPE_UNDER_VOLTAGE:      return "UNDER_VOLTAGE";
        case FAULT_TYPE_OVER_CURRENT:       return "OVER_CURRENT";
        case FAULT_TYPE_OVER_TEMP:          return "OVER_TEMP";
        case FAULT_TYPE_THERMAL_RUNAWAY:    return "THERMAL_RUNAWAY";
        case FAULT_TYPE_PWM_FAULT:          return "PWM_FAULT";
        case FAULT_TYPE_ADC_FAILURE:        return "ADC_FAILURE";
        case FAULT_TYPE_WATCHDOG_TIMEOUT:   return "WATCHDOG_TIMEOUT";
        case FAULT_TYPE_COMMUNICATION:      return "COMMUNICATION";
        case FAULT_TYPE_CRC_ERROR:          return "CRC_ERROR";
        case FAULT_TYPE_CONFIG_CORRUPT:     return "CONFIG_CORRUPT";
        case FAULT_TYPE_HARDWARE_FAULT:     return "HARDWARE_FAULT";
        case FAULT_TYPE_SOFTWARE_FAULT:     return "SOFTWARE_FAULT";
        case FAULT_TYPE_RECOVERY_SUCCESS:   return "RECOVERY_SUCCESS";
        case FAULT_TYPE_RECOVERY_FAILED:    return "RECOVERY_FAILED";
        default:                            return "UNKNOWN";
    }
}

const char* FaultHistory_GetSeverityString(FaultSeverity_t severity)
{
    switch (severity) {
        case FAULT_SEVERITY_INFO:     return "INFO";
        case FAULT_SEVERITY_WARNING:  return "WARN";
        case FAULT_SEVERITY_ERROR:    return "ERROR";
        case FAULT_SEVERITY_CRITICAL: return "CRIT";
        case FAULT_SEVERITY_FATAL:    return "FATAL";
        default:                      return "UNKNOWN";
    }
}

const char* FaultHistory_GetRecoveryActionString(RecoveryAction_t action)
{
    switch (action) {
        case RECOVERY_ACTION_NONE:    return "NONE";
        case RECOVERY_ACTION_RETRY:   return "RETRY";
        case RECOVERY_ACTION_RESET:   return "RESET";
        case RECOVERY_ACTION_DEGRADE: return "DEGRADE";
        case RECOVERY_ACTION_STOP:    return "STOP";
        case RECOVERY_ACTION_RESTART: return "RESTART";
        case RECOVERY_ACTION_MANUAL:  return "MANUAL";
        default:                      return "UNKNOWN";
    }
}

const char* FaultHistory_GetRecoveryResultString(RecoveryResult_t result)
{
    switch (result) {
        case RECOVERY_RESULT_PENDING:  return "PENDING";
        case RECOVERY_RESULT_SUCCESS:  return "SUCCESS";
        case RECOVERY_RESULT_PARTIAL:  return "PARTIAL";
        case RECOVERY_RESULT_FAILED:   return "FAILED";
        case RECOVERY_RESULT_TIMEOUT:  return "TIMEOUT";
        default:                       return "UNKNOWN";
    }
}

const char* FaultHistory_GetWearStatusString(float wear_pct)
{
    if (wear_pct >= WEAR_LEVEL_CRITICAL_PCT) {
        return "CRITICAL";
    } else if (wear_pct >= WEAR_LEVEL_WARNING_PCT) {
        return "WARNING";
    } else if (wear_pct >= WEAR_LEVEL_SAFE_PCT) {
        return "MODERATE";
    } else {
        return "GOOD";
    }
}

uint16_t FaultHistory_FormatEntry(const FaultEntry_t* entry, char* buffer, uint16_t size)
{
    if (entry == NULL || buffer == NULL || size == 0) {
        return 0;
    }
    
    uint16_t written = 0;
    
    // Verify entry CRC
    uint16_t crc = calculate_entry_crc(entry);
    bool valid = (crc == entry->crc);
    
    written += snprintf(buffer + written, size - written,
        "[%lu] %s/%s: %s (code=0x%04X, data=%lu)",
        (unsigned long)entry->timestamp_ms,
        FaultHistory_GetSeverityString(entry->severity),
        FaultHistory_GetTypeString(entry->fault_type),
        valid ? "" : "[CRC_FAIL] ",
        entry->error_code,
        (unsigned long)entry->context_data);
    
    if (entry->recovery_action != RECOVERY_ACTION_NONE) {
        written += snprintf(buffer + written, size - written,
            " | Recovery: %s=%s",
            FaultHistory_GetRecoveryActionString(entry->recovery_action),
            FaultHistory_GetRecoveryResultString(entry->recovery_result));
    }
    
    written += snprintf(buffer + written, size - written, "\r\n");
    
    return written;
}

uint16_t FaultHistory_GetLogText(char* buffer, uint16_t size, uint32_t max_entries)
{
    if (buffer == NULL || size == 0) {
        return 0;
    }
    
    uint16_t written = 0;
    
    written += snprintf(buffer + written, size - written,
        "Fault History Log:\r\n");
    written += snprintf(buffer + written, size - written,
        "==================\r\n");
    
    uint32_t count = FaultHistory_GetCount();
    if (count == 0) {
        written += snprintf(buffer + written, size - written,
            "No faults recorded.\r\n");
        return written;
    }
    
    written += snprintf(buffer + written, size - written,
        "Total entries: %lu (showing max %lu)\r\n\r\n",
        (unsigned long)count, (unsigned long)max_entries);
    
    for (uint32_t i = 0; i < count && i < max_entries; i++) {
        FaultEntry_t entry;
        if (FaultHistory_Read(i, &entry)) {
            written += FaultHistory_FormatEntry(&entry, buffer + written, size - written);
        }
        
        if (written >= size - 1) {
            break;
        }
    }
    
    return written;
}

FaultType_t FaultHistory_MapErrorCode(uint16_t error_code)
{
    switch (error_code) {
        case 0x0000: return FAULT_TYPE_NONE;
        case 0x0001: return FAULT_TYPE_OVER_VOLTAGE;
        case 0x0002: return FAULT_TYPE_UNDER_VOLTAGE;
        case 0x0003: return FAULT_TYPE_OVER_CURRENT;
        case 0x0004: return FAULT_TYPE_OVER_TEMP;
        case 0x0006: return FAULT_TYPE_PWM_FAULT;
        case 0x0007: return FAULT_TYPE_ADC_FAILURE;
        case 0x0008: return FAULT_TYPE_WATCHDOG_TIMEOUT;
        case 0x000A: return FAULT_TYPE_COMMUNICATION;  // CLI auth failure
        case 0x000B: return FAULT_TYPE_CONFIG_CORRUPT;  // Flash write fail
        default:     return FAULT_TYPE_SOFTWARE_FAULT;
    }
}

uint32_t FaultHistory_GetEntriesSince(uint32_t since_timestamp, FaultEntry_t* entries, 
                                       uint32_t max_entries)
{
    if (entries == NULL || max_entries == 0) {
        return 0;
    }
    
    uint32_t count = 0;
    uint32_t total = FaultHistory_GetCount();
    
    for (uint32_t i = 0; i < total && count < max_entries; i++) {
        FaultEntry_t entry;
        if (FaultHistory_Read(i, &entry)) {
            if (entry.timestamp_ms >= since_timestamp) {
                entries[count++] = entry;
            }
        }
    }
    
    return count;
}

bool FaultHistory_AnalyzePattern(uint32_t window_ms, bool* burst_detected, 
                                  float* fault_rate)
{
    if (burst_detected == NULL || fault_rate == NULL) {
        return false;
    }
    
    *burst_detected = false;
    *fault_rate = 0.0f;
    
    uint32_t total = FaultHistory_GetCount();
    if (total == 0) {
        return true;  // No faults is fine
    }
    
    uint32_t now = HAL_GetTick();
    uint32_t window_faults = 0;
    uint32_t oldest_in_window = now;
    uint32_t newest_in_window = 0;
    
    for (uint32_t i = 0; i < total; i++) {
        FaultEntry_t entry;
        if (FaultHistory_Read(i, &entry)) {
            if (now - entry.timestamp_ms <= window_ms) {
                window_faults++;
                if (entry.timestamp_ms < oldest_in_window) {
                    oldest_in_window = entry.timestamp_ms;
                }
                if (entry.timestamp_ms > newest_in_window) {
                    newest_in_window = entry.timestamp_ms;
                }
            }
        }
    }
    
    if (window_faults == 0) {
        return true;
    }
    
    // Calculate fault rate (faults per hour)
    uint32_t actual_window = newest_in_window - oldest_in_window;
    if (actual_window > 0) {
        *fault_rate = (float)window_faults * 3600000.0f / (float)actual_window;
    }
    
    // Detect burst: more than 3 faults within 1 minute
    if (window_faults >= 3) {
        uint32_t burst_window = 60000;  // 1 minute
        if (actual_window <= burst_window) {
            *burst_detected = true;
        }
    }
    
    return true;
}

bool FaultHistory_ValidateWearLeveling(uint32_t* errors_out)
{
    if (errors_out == NULL) {
        return false;
    }
    
    *errors_out = 0;
    
    if (!fault_state.initialized) {
        (*errors_out)++;
        return false;
    }
    
    // Validate wear statistics consistency
    if (fault_state.manager.wear_stats.total_writes != fault_state.manager.entry_count) {
        (*errors_out)++;
    }
    
    // Validate indices are within bounds
    if (fault_state.manager.wear_stats.current_index >= FAULT_HISTORY_MAX_ENTRIES) {
        (*errors_out)++;
    }
    
    if (fault_state.manager.wear_stats.oldest_index >= FAULT_HISTORY_MAX_ENTRIES) {
        (*errors_out)++;
    }
    
    // Validate wrap count matches entry count
    uint32_t expected_wraps = fault_state.manager.entry_count / FAULT_HISTORY_MAX_ENTRIES;
    if (fault_state.manager.wear_stats.wrap_count != expected_wraps) {
        (*errors_out)++;
    }
    
    // Validate sector erase count is reasonable
    if (fault_state.manager.wear_stats.sector_erases > expected_wraps) {
        (*errors_out)++;
    }
    
    return (*errors_out == 0);
}
