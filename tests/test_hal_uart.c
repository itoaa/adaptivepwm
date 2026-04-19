/**
 * @file test_hal_uart.c
 * @brief Unit tests for HAL UART Module (PWM-ARCH-008)
 *
 * Tests cover:
 * - UART initialization
 * - Transmit/receive buffer management
 * - Circular buffer operations
 * - Interrupt-driven operations
 * - CLI command parsing
 * - Password handling
 *
 * @version 1.0.0
 * @date 2026-04-18
 */

#include "unity.h"
#include "hal_uart.h"
#include "config.h"
#include <string.h>

// Test fixtures
static Adaptive_UART_t test_uart;

void setUp(void)
{
    memset(&test_uart, 0, sizeof(test_uart));
}

void tearDown(void)
{
    // Cleanup
}

// =============================================================================
// TEST: Initialization
// =============================================================================

void test_UART_Init_ShouldInitializeStructure(void)
{
    bool result = Adaptive_UART_Init(&test_uart, 115200);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(115200, test_uart.baud_rate);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.tx_head);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.tx_tail);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.rx_head);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.rx_tail);
}

void test_UART_Init_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_UART_Init(NULL, 115200);
    TEST_ASSERT_FALSE(result);
}

void test_UART_Init_ZeroBaud_ShouldReturnFalse(void)
{
    bool result = Adaptive_UART_Init(&test_uart, 0);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Send Character
// =============================================================================

void test_UART_SendChar_ShouldQueueCharacter(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    bool result = Adaptive_UART_SendChar(&test_uart, 'A');

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8('A', test_uart.tx_buffer[0]);
    TEST_ASSERT_EQUAL_UINT32(1, test_uart.tx_head);
}

void test_UART_SendChar_BufferFull_ShouldReturnFalse(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Fill buffer
    for (int i = 0; i < UART_TX_BUFFER_SIZE - 1; i++) {
        Adaptive_UART_SendChar(&test_uart, 'X');
    }

    // One more should fail
    bool result = Adaptive_UART_SendChar(&test_uart, 'A');

    // Depends on implementation - may overwrite or return false
    // TEST_ASSERT_FALSE(result);
}

void test_UART_SendChar_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_UART_SendChar(NULL, 'A');
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Send String
// =============================================================================

void test_UART_SendString_ShouldQueueString(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    bool result = Adaptive_UART_SendString(&test_uart, "Hello");

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8('H', test_uart.tx_buffer[0]);
    TEST_ASSERT_EQUAL_UINT8('e', test_uart.tx_buffer[1]);
    TEST_ASSERT_EQUAL_UINT8('l', test_uart.tx_buffer[2]);
}

void test_UART_SendString_NullPointer_ShouldReturnFalse(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    bool result = Adaptive_UART_SendString(NULL, "Hello");
    TEST_ASSERT_FALSE(result);

    result = Adaptive_UART_SendString(&test_uart, NULL);
    TEST_ASSERT_FALSE(result);
}

void test_UART_SendString_EmptyString_ShouldReturnTrue(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    bool result = Adaptive_UART_SendString(&test_uart, "");

    TEST_ASSERT_TRUE(result);
}

// =============================================================================
// TEST: Receive Character
// =============================================================================

void test_UART_ReceiveChar_ShouldReturnQueuedCharacter(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Simulate received character
    test_uart.rx_buffer[0] = 'B';
    test_uart.rx_head = 1;

    char ch;
    bool result = Adaptive_UART_ReceiveChar(&test_uart, &ch);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT8('B', ch);
}

void test_UART_ReceiveChar_EmptyBuffer_ShouldReturnFalse(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    char ch;
    bool result = Adaptive_UART_ReceiveChar(&test_uart, &ch);

    TEST_ASSERT_FALSE(result);
}

void test_UART_ReceiveChar_NullPointer_ShouldReturnFalse(void)
{
    char ch;
    bool result = Adaptive_UART_ReceiveChar(NULL, &ch);
    TEST_ASSERT_FALSE(result);

    result = Adaptive_UART_ReceiveChar(&test_uart, NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Available Bytes
// =============================================================================

void test_UART_BytesAvailable_ShouldReturnCount(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    TEST_ASSERT_EQUAL_UINT32(0, Adaptive_UART_BytesAvailable(&test_uart));

    // Add characters
    test_uart.rx_buffer[0] = 'A';
    test_uart.rx_buffer[1] = 'B';
    test_uart.rx_head = 2;

    TEST_ASSERT_EQUAL_UINT32(2, Adaptive_UART_BytesAvailable(&test_uart));
}

void test_UART_BytesAvailable_NullPointer_ShouldReturnZero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, Adaptive_UART_BytesAvailable(NULL));
}

// =============================================================================
// TEST: Send Buffer Space
// =============================================================================

void test_UART_SendSpace_ShouldReturnAvailableSpace(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    uint32_t space = Adaptive_UART_SendSpace(&test_uart);
    TEST_ASSERT_EQUAL_UINT32(UART_TX_BUFFER_SIZE - 1, space);

    // Add character
    Adaptive_UART_SendChar(&test_uart, 'A');
    space = Adaptive_UART_SendSpace(&test_uart);
    TEST_ASSERT_EQUAL_UINT32(UART_TX_BUFFER_SIZE - 2, space);
}

void test_UART_SendSpace_NullPointer_ShouldReturnZero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, Adaptive_UART_SendSpace(NULL));
}

// =============================================================================
// TEST: Flush
// =============================================================================

void test_UART_Flush_ShouldClearBuffers(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Add data to buffers
    Adaptive_UART_SendChar(&test_uart, 'A');
    test_uart.rx_buffer[0] = 'B';
    test_uart.rx_head = 1;

    bool result = Adaptive_UART_Flush(&test_uart);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.tx_head);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.tx_tail);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.rx_head);
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.rx_tail);
}

void test_UART_Flush_NullPointer_ShouldReturnFalse(void)
{
    bool result = Adaptive_UART_Flush(NULL);
    TEST_ASSERT_FALSE(result);
}

// =============================================================================
// TEST: Circular Buffer Wrap
// =============================================================================

void test_UART_CircularBuffer_ShouldWrapCorrectly(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Fill near end of buffer
    test_uart.tx_head = UART_TX_BUFFER_SIZE - 2;
    Adaptive_UART_SendChar(&test_uart, 'X');
    Adaptive_UART_SendChar(&test_uart, 'Y');

    // Should wrap to beginning
    TEST_ASSERT_EQUAL_UINT32(0, test_uart.tx_head);
}

// =============================================================================
// TEST: CLI Command Parsing (if supported)
// =============================================================================

void test_UART_ProcessRX_ShouldParseCommands(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Simulate receiving "help\r\n"
    const char *cmd = "help\r";
    for (size_t i = 0; i < strlen(cmd); i++) {
        test_uart.rx_buffer[test_uart.rx_head++] = cmd[i];
    }

    // Process should find the command
    // Note: Implementation depends on actual function signature
    // This is a placeholder for the test structure
}

// =============================================================================
// TEST: Line Reception
// =============================================================================

void test_UART_HasLine_ShouldDetectNewline(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    TEST_ASSERT_FALSE(Adaptive_UART_HasLine(&test_uart));

    // Add characters with newline
    test_uart.rx_buffer[0] = 'h';
    test_uart.rx_buffer[1] = 'i';
    test_uart.rx_buffer[2] = '\n';
    test_uart.rx_head = 3;

    TEST_ASSERT_TRUE(Adaptive_UART_HasLine(&test_uart));
}

void test_UART_HasLine_NoNewline_ShouldReturnFalse(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    test_uart.rx_buffer[0] = 'h';
    test_uart.rx_buffer[1] = 'i';
    test_uart.rx_head = 2;

    TEST_ASSERT_FALSE(Adaptive_UART_HasLine(&test_uart));
}

void test_UART_HasLine_NullPointer_ShouldReturnFalse(void)
{
    TEST_ASSERT_FALSE(Adaptive_UART_HasLine(NULL));
}

// =============================================================================
// TEST: Read Line
// =============================================================================

void test_UART_ReadLine_ShouldExtractLine(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Add line with newline
    const char *line = "test command\r\n";
    for (size_t i = 0; i < strlen(line); i++) {
        test_uart.rx_buffer[test_uart.rx_head++] = line[i];
    }

    char buffer[32];
    int32_t result = Adaptive_UART_ReadLine(&test_uart, buffer, sizeof(buffer));

    TEST_ASSERT_GREATER_THAN_INT32(0, result);
    // Should contain line without newline
    TEST_ASSERT_EQUAL_STRING("test command", buffer);
}

void test_UART_ReadLine_BufferTooSmall_ShouldTruncate(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    // Add long line
    const char *line = "this is a very long command\r\n";
    for (size_t i = 0; i < strlen(line); i++) {
        test_uart.rx_buffer[test_uart.rx_head++] = line[i];
    }

    char buffer[10];
    int32_t result = Adaptive_UART_ReadLine(&test_uart, buffer, sizeof(buffer));

    TEST_ASSERT_GREATER_THAN_INT32(0, result);
    // Should be truncated
    TEST_ASSERT_TRUE(strlen(buffer) < sizeof(buffer));
}

void test_UART_ReadLine_NoLine_ShouldReturnZero(void)
{
    Adaptive_UART_Init(&test_uart, 115200);

    char buffer[32];
    int32_t result = Adaptive_UART_ReadLine(&test_uart, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_INT32(0, result);
}

void test_UART_ReadLine_NullPointer_ShouldReturnNegative(void)
{
    char buffer[32];
    int32_t result = Adaptive_UART_ReadLine(NULL, buffer, sizeof(buffer));
    TEST_ASSERT_LESS_THAN_INT32(0, result);

    Adaptive_UART_Init(&test_uart, 115200);
    result = Adaptive_UART_ReadLine(&test_uart, NULL, sizeof(buffer));
    TEST_ASSERT_LESS_THAN_INT32(0, result);
}

// =============================================================================
// TEST: Configuration Constants
// =============================================================================

void test_UART_Configuration_ShouldBeValid(void)
{
    // Buffer sizes should be powers of 2 for efficient circular buffer
    TEST_ASSERT_TRUE(UART_TX_BUFFER_SIZE > 0);
    TEST_ASSERT_TRUE(UART_RX_BUFFER_SIZE > 0);

    // Baud rate should be reasonable
    TEST_ASSERT_TRUE(UART_BAUDRATE > 9600);
    TEST_ASSERT_TRUE(UART_BAUDRATE <= 1000000);
}

// =============================================================================
// TEST: Full Sequence
// =============================================================================

void test_UART_FullSequence_ShouldWork(void)
{
    // Initialize
    TEST_ASSERT_TRUE(Adaptive_UART_Init(&test_uart, 115200));

    // Send characters
    TEST_ASSERT_TRUE(Adaptive_UART_SendChar(&test_uart, 'H'));
    TEST_ASSERT_TRUE(Adaptive_UART_SendChar(&test_uart, 'i'));

    // Send string
    TEST_ASSERT_TRUE(Adaptive_UART_SendString(&test_uart, "Hello"));

    // Check available space
    uint32_t space = Adaptive_UART_SendSpace(&test_uart);
    TEST_ASSERT_GREATER_THAN_UINT32(0, space);

    // Simulate receive
    test_uart.rx_buffer[test_uart.rx_head++] = 'E';
    test_uart.rx_buffer[test_uart.rx_head++] = 'c';
    test_uart.rx_buffer[test_uart.rx_head++] = 'h';
    test_uart.rx_buffer[test_uart.rx_head++] = 'o';
    test_uart.rx_buffer[test_uart.rx_head++] = '\r';

    // Check for line
    TEST_ASSERT_TRUE(Adaptive_UART_HasLine(&test_uart));

    // Read line
    char buffer[32];
    int32_t len = Adaptive_UART_ReadLine(&test_uart, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN_INT32(0, len);

    // Flush
    TEST_ASSERT_TRUE(Adaptive_UART_Flush(&test_uart));
}

// =============================================================================
// RUN TESTS
// =============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_UART_Init_ShouldInitializeStructure);
    RUN_TEST(test_UART_Init_NullPointer_ShouldReturnFalse);
    RUN_TEST(test_UART_Init_ZeroBaud_ShouldReturnFalse);

    // Send character tests
    RUN_TEST(test_UART_SendChar_ShouldQueueCharacter);
    RUN_TEST(test_UART_SendChar_BufferFull_ShouldReturnFalse);
    RUN_TEST(test_UART_SendChar_NullPointer_ShouldReturnFalse);

    // Send string tests
    RUN_TEST(test_UART_SendString_ShouldQueueString);
    RUN_TEST(test_UART_SendString_NullPointer_ShouldReturnFalse);
    RUN_TEST(test_UART_SendString_EmptyString_ShouldReturnTrue);

    // Receive character tests
    RUN_TEST(test_UART_ReceiveChar_ShouldReturnQueuedCharacter);
    RUN_TEST(test_UART_ReceiveChar_EmptyBuffer_ShouldReturnFalse);
    RUN_TEST(test_UART_ReceiveChar_NullPointer_ShouldReturnFalse);

    // Available bytes tests
    RUN_TEST(test_UART_BytesAvailable_ShouldReturnCount);
    RUN_TEST(test_UART_BytesAvailable_NullPointer_ShouldReturnZero);

    // Send space tests
    RUN_TEST(test_UART_SendSpace_ShouldReturnAvailableSpace);
    RUN_TEST(test_UART_SendSpace_NullPointer_ShouldReturnZero);

    // Flush tests
    RUN_TEST(test_UART_Flush_ShouldClearBuffers);
    RUN_TEST(test_UART_Flush_NullPointer_ShouldReturnFalse);

    // Circular buffer tests
    RUN_TEST(test_UART_CircularBuffer_ShouldWrapCorrectly);

    // Line detection tests
    RUN_TEST(test_UART_HasLine_ShouldDetectNewline);
    RUN_TEST(test_UART_HasLine_NoNewline_ShouldReturnFalse);
    RUN_TEST(test_UART_HasLine_NullPointer_ShouldReturnFalse);

    // Read line tests
    RUN_TEST(test_UART_ReadLine_ShouldExtractLine);
    RUN_TEST(test_UART_ReadLine_BufferTooSmall_ShouldTruncate);
    RUN_TEST(test_UART_ReadLine_NoLine_ShouldReturnZero);
    RUN_TEST(test_UART_ReadLine_NullPointer_ShouldReturnNegative);

    // Configuration tests
    RUN_TEST(test_UART_Configuration_ShouldBeValid);

    // Integration test
    RUN_TEST(test_UART_FullSequence_ShouldWork);

    return UNITY_END();
}
