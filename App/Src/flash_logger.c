/**
 * @file flash_logger.c
 * @brief Flash logging implementation with wear leveling
 */

#include "flash_logger.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    uint32_t magic;
    uint32_t write_index;
    uint32_t entry_count;
    uint32_t wrap_count;
} FlashHeader_t;

static uint32_t CalculateCRC(const LogEntry_t* entry)
{
    uint32_t crc = 0;
    const uint8_t* data = (const uint8_t*)entry;
    size_t len = sizeof(LogEntry_t) - sizeof(uint16_t);
    for (size_t i = 0; i < len; i++) {
        crc = crc * 31 + data[i];
    }
    return crc & 0xFFFF;
}

static bool EraseSector(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
    
    HAL_FLASH_Unlock();
    
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_LOG_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    bool result = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;
    
    HAL_FLASH_Lock();
    return result;
}

bool FlashLogger_Init(FlashLogger_t* logger)
{
    if (logger == NULL) return false;
    
    memset(logger, 0, sizeof(FlashLogger_t));
    
    // Check for existing log
    FlashHeader_t* header = (FlashHeader_t*)FLASH_LOG_START_ADDR;
    
    if (header->magic == FLASH_MAGIC) {
        logger->write_index = header->write_index;
        logger->entry_count = header->entry_count;
        logger->wrap_count = header->wrap_count;
        logger->initialized = true;
    } else {
        // Initialize new log
        logger->write_index = sizeof(FlashHeader_t);
        logger->entry_count = 0;
        logger->wrap_count = 0;
        logger->initialized = true;
    }
    
    return true;
}

static uint32_t GetNextWriteAddress(FlashLogger_t* logger)
{
    // Simple wear leveling: distribute writes across sector
    // Calculate next position based on wrap count
    uint32_t base = FLASH_LOG_START_ADDR + sizeof(FlashHeader_t);
    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeader_t)) / FLASH_LOG_ENTRY_SIZE;
    
    // Spread writes evenly
    uint32_t offset = (logger->entry_count + logger->wrap_count) % max_entries;
    
    return base + (offset * FLASH_LOG_ENTRY_SIZE);
}

bool FlashLogger_Write(FlashLogger_t* logger, const LogEntry_t* entry)
{
    if (logger == NULL || entry == NULL || !logger->initialized) return false;
    
    LogEntry_t write_entry = *entry;
    write_entry.crc = CalculateCRC(&write_entry);
    
    uint32_t addr = GetNextWriteAddress(logger);
    
    // Check if we need to wrap (sector full)
    if (addr + sizeof(LogEntry_t) > FLASH_LOG_START_ADDR + FLASH_LOG_SIZE) {
        logger->wrap_count++;
        addr = FLASH_LOG_START_ADDR + sizeof(FlashHeader_t);
    }
    
    HAL_FLASH_Unlock();
    
    // Write entry word by word
    uint32_t* data = (uint32_t*)&write_entry;
    size_t num_words = sizeof(LogEntry_t) / 4;
    for (size_t i = 0; i < num_words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, data[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    
    logger->entry_count++;
    
    return true;
}

uint32_t FlashLogger_Read(FlashLogger_t* logger, uint32_t index, LogEntry_t* entry)
{
    if (logger == NULL || entry == NULL || !logger->initialized) return 0;
    if (index >= logger->entry_count) return 0;
    
    // Calculate actual address with wear leveling
    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeader_t)) / FLASH_LOG_ENTRY_SIZE;
    uint32_t actual_idx = (logger->entry_count - index - 1) % max_entries;
    uint32_t addr = FLASH_LOG_START_ADDR + sizeof(FlashHeader_t) + (actual_idx * FLASH_LOG_ENTRY_SIZE);
    
    memcpy(entry, (void*)addr, sizeof(LogEntry_t));
    
    // Verify CRC
    if (entry->crc != CalculateCRC(entry)) {
        return 0;  // Corrupt entry
    }
    
    return sizeof(LogEntry_t);
}

bool FlashLogger_Clear(FlashLogger_t* logger)
{
    if (logger == NULL) return false;
    
    if (!EraseSector()) return false;
    
    logger->write_index = sizeof(FlashHeader_t);
    logger->entry_count = 0;
    logger->wrap_count = 0;
    
    // Write new header
    HAL_FLASH_Unlock();
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_LOG_START_ADDR, FLASH_MAGIC);
    HAL_FLASH_Lock();
    
    return true;
}

uint32_t FlashLogger_GetEntryCount(const FlashLogger_t* logger)
{
    if (logger == NULL) return 0;
    return logger->entry_count;
}

void FlashLogger_GetStats(const FlashLogger_t* logger, char* buffer, uint16_t size)
{
    if (logger == NULL || buffer == NULL) return;
    
    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeader_t)) / FLASH_LOG_ENTRY_SIZE;
    
    snprintf(buffer, size,
        "Flash Log:\r\n"
        "  Entries: %lu/%lu\r\n"
        "  Wraps: %lu\r\n"
        "  Wear level: %lu%%",
        (unsigned long)logger->entry_count,
        (unsigned long)max_entries,
        (unsigned long)logger->wrap_count,
        (unsigned long)((logger->entry_count % max_entries) * 100 / max_entries));
}
