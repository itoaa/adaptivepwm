#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* Minimal mbedtls config for embedded STM32 */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

/* Disable platform entropy - use custom entropy source */
#define MBEDTLS_NO_PLATFORM_ENTROPY

/* Enable required algorithms */
#define MBEDTLS_SHA256_C
#define MBEDTLS_MD_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_AES_C

/* Disable features not needed for embedded */
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_FS_IO

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
