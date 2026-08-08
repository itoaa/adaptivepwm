/**
 * @file stm32f4xx_hal_conf.h
 * @brief HAL configuration for AdaptivePWM (NUCLEO-F401RE, 16 MHz HSE)
 */
#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#if !defined(HSE_VALUE)
#define HSE_VALUE 16000000U
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE 32000U
#endif
#if !defined(LSE_VALUE)
#define LSE_VALUE 32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT 5000U
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE 12288000U
#endif

#define VDD_VALUE                 3300U
#define TICK_INT_PRIORITY         0U
#define USE_RTOS                  0U
#define PREFETCH_ENABLE           1U
#define INSTRUCTION_CACHE_ENABLE  1U
#define DATA_CACHE_ENABLE         1U

#define USE_HAL_ADC_REGISTER_CALLBACKS     0U
#define USE_HAL_TIM_REGISTER_CALLBACKS     0U
#define USE_HAL_UART_REGISTER_CALLBACKS    0U

#define MAC_ADDR0 2U
#define MAC_ADDR1 0U
#define MAC_ADDR2 0U
#define MAC_ADDR3 0U
#define MAC_ADDR4 0U
#define MAC_ADDR5 0U

#define ETH_RX_BUF_SIZE ETH_MAX_PACKET_SIZE
#define ETH_TX_BUF_SIZE ETH_MAX_PACKET_SIZE
#define ETH_RXBUFNB     4U
#define ETH_TXBUFNB     4U

#define DP83848_PHY_ADDRESS 0x01U
#define PHY_RESET_DELAY     0x000000FFU
#define PHY_CONFIG_DELAY    0x00000FFFU
#define PHY_READ_TO         0x0000FFFFU
#define PHY_WRITE_TO        0x0000FFFFU
#define PHY_BCR             0x00U
#define PHY_BSR             0x01U
#define PHY_RESET           0x8000U
#define PHY_LOOPBACK        0x4000U
#define PHY_FULLDUPLEX_100M 0x2100U
#define PHY_HALFDUPLEX_100M 0x2000U
#define PHY_FULLDUPLEX_10M  0x0100U
#define PHY_HALFDUPLEX_10M  0x0000U
#define PHY_AUTONEGOTIATION 0x1000U
#define PHY_RESTART_AUTONEGOTIATION 0x0200U
#define PHY_POWERDOWN       0x0800U
#define PHY_ISOLATE         0x0400U
#define PHY_AUTONEGO_COMPLETE 0x0020U
#define PHY_LINKED_STATUS   0x0004U
#define PHY_JABBER_DETECTION 0x0002U
#define PHY_SR              0x10U
#define PHY_SPEED_STATUS    0x0002U
#define PHY_DUPLEX_STATUS   0x0004U

#define USE_SPI_CRC 0U

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f4xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f4xx_hal_gpio.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32f4xx_hal_exti.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f4xx_hal_cortex.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32f4xx_hal_adc.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f4xx_hal_pwr.h"
#endif
#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32f4xx_hal_iwdg.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f4xx_hal_tim.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f4xx_hal_uart.h"
#endif

#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_HAL_CONF_H */
