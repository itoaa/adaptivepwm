/**
 * @file flash_logger.h
 * @brief Persistent data logging to internal flash
 */

#ifndef FLASH_LOGGER_H
#define FLASH_LOGGER_H

#include <stdint.h>
#include <stdbool.h>

// Flash configuration - STM32F401 has sectors 0-7 (128KB each for large sectors)
// Using last available sector
#define FLASH_LOG_START_ADDR    0x080E0000  // Sector 7 (last 128KB)
#define FLASH_LOG_SECTOR        FLASH_SECTOR_7
#define FLASH_LOG_SIZE          8192        // 8KB log space
#define FLASH_LOG_ENTRY_SIZE    32

typedef struct {
    uint32_t timestamp;
    float duty_cycle;
    float efficiency;
    float temperature;
    float current;
    uint16_t error_code;
    uint16_t crc;
} LogEntry_t;

typedef struct {
    uint32_t write_index;
    uint32_t entry_count;
    uint32_t wrap_count;
    bool initialized;
} FlashLogger_t;

bool FlashLogger_Init(FlashLogger_t* logger);
bool FlashLogger_Write(FlashLogger_t* logger, const LogEntry_t* entry);
uint32_t FlashLogger_Read(FlashLogger_t* logger, uint32_t index, LogEntry_t* entry);
bool FlashLogger_Clear(FlashLogger_t* logger);
uint32_t FlashLogger_GetEntryCount(const FlashLogger_t* logger);
void FlashLogger_GetStats(const FlashLogger_t* logger, char* buffer, uint16_t size);

#endif // FLASH_LOGGER_H
