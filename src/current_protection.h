/**
 * @file current_protection.h
 * @brief Hardware-based overcurrent protection
 * 
 * Uses comparator (COMP) for fast overcurrent detection
 * independent of software polling.
 */

#ifndef CURRENT_PROTECTION_H
#define CURRENT_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>

// COMP configuration
#define COMP_INSTANCE       COMP2
#define COMP_GPIO_PORT    GPIOA
#define COMP_GPIO_PIN     GPIO_PIN_7   // PA7 - COMP2_INP

// Threshold levels (A)
#define OC_THRESHOLD_50     5.0f
#define OC_THRESHOLD_100    10.0f
#define OC_THRESHOLD_150    15.0f
#define OC_THRESHOLD_200    20.0f

typedef enum {
    OC_LEVEL_NONE,
    OC_LEVEL_WARNING,
    OC_LEVEL_SHUTDOWN
} OC_Level_t;

typedef struct {
    float threshold_warning;
    float threshold_shutdown;
    bool enabled;
    OC_Level_t last_level;
    uint32_t trip_count;
} CurrentProtection_t;

bool CurrentProtection_Init(CurrentProtection_t* prot, float threshold);
void CurrentProtection_Enable(CurrentProtection_t* prot);
void CurrentProtection_Disable(CurrentProtection_t* prot);
void CurrentProtection_SetThreshold(CurrentProtection_t* prot, float warning, float shutdown);
OC_Level_t CurrentProtection_Check(CurrentProtection_t* prot, float current);
bool CurrentProtection_GetStatus(const CurrentProtection_t* prot, char* buffer, uint16_t size);

#endif // CURRENT_PROTECTION_H
