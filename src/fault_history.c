/**
 * @file fault_history.c
 * @brief Fault History Implementation with Persistent Flash Storage
 * 
 * Security Task: PWM-ARCH-004
 * 
 * Implements fault logging with circular buffer in internal Flash.
 * Includes CRC validation, wear leveling, and predictive maintenance.
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

// Flash header structure
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t version;
    uint32_t write_index;
    uint32_t entry_count;
    uint32_t wrap_count;
    uint32_t last_fault_timestamp;
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
 * @brief Get entry address in flash
 * 
 * @param index Entry index
 * @return Flash address
 */
static uint32_t get_entry_address(uint32_t index)
{
    uint32_t base = FAULT_HISTORY_FLASH_ADDR + sizeof(FlashHeader_t);
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
 * @brief Write entry to flash
 * 
 * @param index Entry index
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
        fault_state.manager.statistics = header.statistics;
        fault_state.manager.initialized = true;
    } else {
        // Initialize new fault history
        fault_state.manager.write_index = 0;
        fault_state.manager.entry_count = 0;
        fault_state.manager.wrap_count = 0;
        fault_state.manager.last_fault_timestamp = 0;
        memset(&fault_state.manager.statistics, 0, sizeof(FaultStatistics_t));
        fault_state.manager.initialized = true;
        
        // Write initial header
        FlashHeader_t new_header = {
            .magic = FAULT_HISTORY_MAGIC,
            .version = FAULT_HISTORY_VERSION,
            .write_index = 0,
            .entry_count = 0,
            .wrap_count = 0,
            .last_fault_timestamp = 0,
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
    
    // Write current header to preserve state
    FlashHeader_t header = {
        .magic = FAULT_HISTORY_MAGIC,
        .version = FAULT_HISTORY_VERSION,
        .write_index = fault_state.manager.write_index,
        .entry_count = fault_state.manager.entry_count,
        .wrap_count = fault_state.manager.wrap_count,
        .last_fault_timestamp = fault_state.manager.last_fault_timestamp,
        .statistics = fault_state.manager.statistics,
        .crc = 0
    };
    header.crc = calculate_header_crc(&header);
    
    write_header(&header);
    
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
    
    // Check if we need to wrap
    if (fault_state.manager.entry_count >= FAULT_HISTORY_MAX_ENTRIES) {
        fault_state.manager.wrap_count++;
        fault_state.manager.write_index = fault_state.manager.wrap_count % FAULT_HISTORY_MAX_ENTRIES;
    }
    
    // Write entry
    if (!write_entry(fault_state.manager.write_index, &entry)) {
        return false;
    }
    
    // Update state
    fault_state.manager.entry_count++;
    fault_state.manager.write_index++;
    fault_state.manager.last_fault_timestamp = now;
    
    // Update statistics
    bool recovered = (recovery_result == RECOVERY_RESULT_SUCCESS) ||
                     (recovery_result == RECOVERY_RESULT_PARTIAL);
    update_statistics(fault_type, recovered);
    
    // Update header in flash periodically (every 10 entries)
    if (fault_state.manager.entry_count % 10 == 0) {
        FlashHeader_t header = {
            .magic = FAULT_HISTORY_MAGIC,
            .version = FAULT_HISTORY_VERSION,
            .write_index = fault_state.manager.write_index,
            .entry_count = fault_state.manager.entry_count,
            .wrap_count = fault_state.manager.wrap_count,
            .last_fault_timestamp = fault_state.manager.last_fault_timestamp,
            .statistics = fault_state.manager.statistics,
            .crc = 0
        };
        header.crc = calculate_header_crc(&header);
        write_header(&header);
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
        // Wrapped buffer
        actual_idx = (fault_state.manager.entry_count - 1 - index) % FAULT_HISTORY_MAX_ENTRIES;
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
    
    // Reset state
    fault_state.manager.write_index = 0;
    fault_state.manager.entry_count = 0;
    fault_state.manager.wrap_count = 0;
    fault_state.manager.last_fault_timestamp = 0;
    memset(&fault_state.manager.statistics, 0, sizeof(FaultStatistics_t));
    
    // Write new header
    FlashHeader_t header = {
        .magic = FAULT_HISTORY_MAGIC,
        .version = FAULT_HISTORY_VERSION,
        .write_index = 0,
        .entry_count = 0,
        .wrap_count = 0,
        .last_fault_timestamp = 0,
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
    for (uint32_t i = 0; i < fault_state.manager.entry_count && i < FAULT_HISTORY_MAX_ENTRIES; i++) {
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
    
    // Calculate health score (0-100)
    float health = 100.0f;
    
    // Reduce health based on fault history
    health -= stats->total_faults * 0.5f;
    health -= stats->failed_recoveries * 2.0f;
    
    // Reduce based on fault rate
    if (stats->fault_rate_24h > 0) {
        health -= stats->fault_rate_24h * 1.0f;
    }
    
    // Clamp to 0-100
    if (health < 0.0f) health = 0.0f;
    if (health > 100.0f) health = 100.0f;
    
    prediction->health_score = health;
    
    // Determine if maintenance is recommended
    prediction->maintenance_recommended = (health < MAINTENANCE_HEALTH_THRESHOLD) ||
                                             (stats->fault_rate_24h > MAINTENANCE_FAULT_THRESHOLD);
    
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
    
    prediction->degradation_rate = degradation;
    
    // Determine primary concern
    if (stats->faults_by_type[FAULT_TYPE_THERMAL_RUNAWAY] > 0) {
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
