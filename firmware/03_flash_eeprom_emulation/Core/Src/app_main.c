/*
 * app_main.c
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "app_main.h"
#include "uart_log.h"
#include "ee_format.h"
#include "ee_storage.h"

/* Private defines -----------------------------------------------------------*/
#define APP_TEST_MODE_BOOT_CHECK              0
#define APP_TEST_MODE_FORMAT_DEFAULT          1
#define APP_TEST_MODE_WRITE_READBACK          2
#define APP_TEST_MODE_APPEND_LATEST           3
#define APP_TEST_MODE_PAGE_TRANSFER           4
#define APP_TEST_MODE_FAULT_AFTER_RECEIVE     5
#define APP_TEST_MODE_FAULT_AFTER_COPY        6
#define APP_TEST_MODE_CORRUPT_RECORD          7
#define APP_TEST_MODE_FAULT_AFTER_PROGRAM     8

// Set default test mode
#define APP_TEST_MODE                         APP_TEST_MODE_WRITE_READBACK

// Define LED for heartbeat
#define APP_LED_GPIO_Port                     GPIOA
#define APP_LED_Pin                           GPIO_PIN_5

// Define heartbeat period
#define APP_HEARTBEAT_PERIOD                  1000U

/* Private variables ---------------------------------------------------------*/
static uint8_t boot_log_done = 0U;
static uint8_t boot_check_done = 0U;
static uint8_t format_test_done = 0U;
static uint8_t write_readback_test_done = 0U;
static uint8_t not_implemented_log_done = 0U;

/* Private function prototypes -----------------------------------------------*/
static const char *App_GetTestModeName(uint32_t test_mode);
static void App_PrintBootLog(void);
static void App_RunBootCheck(void);
static void App_RunFormatDefault(void);
static void App_RunWriteReadback(void);
static void App_RunNotImplemented(void);
static void App_UpdateHeartbeat(void);

/* Private functions ---------------------------------------------------------*/

// Convert test mode number to readable name
static const char *App_GetTestModeName(uint32_t test_mode)
{
  switch (test_mode)
  {
    case APP_TEST_MODE_BOOT_CHECK:
      return "boot_check";

    case APP_TEST_MODE_FORMAT_DEFAULT:
      return "format_default";

    case APP_TEST_MODE_WRITE_READBACK:
      return "write_readback";

    case APP_TEST_MODE_APPEND_LATEST:
      return "append_latest";

    case APP_TEST_MODE_PAGE_TRANSFER:
      return "page_transfer";

    case APP_TEST_MODE_FAULT_AFTER_RECEIVE:
      return "fault_after_receive";

    case APP_TEST_MODE_FAULT_AFTER_COPY:
      return "fault_after_copy";

    case APP_TEST_MODE_CORRUPT_RECORD:
      return "corrupt_record";

    case APP_TEST_MODE_FAULT_AFTER_PROGRAM:
      return "fault_after_program";

    default:
      return "unknown";
  }
}

// Print boot information
static void App_PrintBootLog(void)
{
  // Run this test only once
  if (boot_log_done != 0U) {
    return;
  }

  // Mark test as done
  boot_log_done = 1U;

  // Print blank line after reset
  UartLog_Printf("\r\n");

  // Print project information
  UartLog_Printf("[BOOT] STM32G0B1RE Firmware Debugging Lab\r\n");
  UartLog_Printf("[BOOT] Project: 03_flash_eeprom_emulation\r\n");
  UartLog_Printf("[BOOT] Board: NUCLEO-G0B1RE\r\n");

  // Print clock and test mode
  UartLog_Printf("[BOOT] System clock: %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
  UartLog_Printf("[BOOT] Test mode: %s\r\n", App_GetTestModeName(APP_TEST_MODE));
}

// Run boot check test
static void App_RunBootCheck(void)
{
  // Run this test only once
  if (boot_check_done != 0U) {
    return;
  }

  // Mark test as done
  boot_check_done = 1U;

  // Print EEPROM page addresses
  UartLog_Printf("[TEST0] EE_PAGE_A_ADDR=0x%08lX\r\n", (unsigned long)EE_PAGE_A_ADDR);
  UartLog_Printf("[TEST0] EE_PAGE_B_ADDR=0x%08lX\r\n", (unsigned long)EE_PAGE_B_ADDR);

  // Print EEPROM layout
  UartLog_Printf("[TEST0] page_size=%lu\r\n", (unsigned long)EE_PAGE_SIZE);
  UartLog_Printf("[TEST0] header_size=%lu\r\n", (unsigned long)EE_HEADER_SIZE);
  UartLog_Printf("[TEST0] record_size=%lu\r\n", (unsigned long)EE_RECORD_SIZE);
  UartLog_Printf("[TEST0] records_per_page=%lu\r\n", (unsigned long)EE_RECORDS_PER_PAGE);

  // Check system clock
  if (HAL_RCC_GetSysClockFreq() == 48000000UL) {
    UartLog_Printf("[TEST0] system_clock_check=OK\r\n");
  } else {
    UartLog_Printf("[TEST0] system_clock_check=NG expected=48000000 actual=%lu\r\n",
                   (unsigned long)HAL_RCC_GetSysClockFreq());
  }

  // Print test result
  UartLog_Printf("[TEST0] PASS\r\n");
}

// Run format default test
static void App_RunFormatDefault(void)
{
  EeStatus_t status;

  // Run this test only once
  if (format_test_done != 0U) {
    return;
  }

  // Mark test as done
  format_test_done = 1U;

  // Force format EEPROM area
  status = Ee_Format();
  if (status != EE_OK) {
    UartLog_Printf("[TEST1] format=NG status=%s\r\n", Ee_StatusToString(status));
    return;
  }

  // Verify active page after format
  if ((Ee_GetActivePageAddr() == EE_PAGE_A_ADDR) &&
      (Ee_GetWriteOffset() == EE_HEADER_SIZE)) {
    UartLog_Printf("[TEST1] active_page_check=OK\r\n");
    UartLog_Printf("[TEST1] write_offset_check=OK\r\n");
    UartLog_Printf("[TEST1] PASS\r\n");
  } else {
    UartLog_Printf("[TEST1] active_page_check=NG active=0x%08lX offset=%lu\r\n",
                   (unsigned long)Ee_GetActivePageAddr(),
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST1] FAIL\r\n");
  }
}

// Run write and readback test
static void App_RunWriteReadback(void)
{
  EeStatus_t status;
  uint32_t read_value = 0U;

  // Run this test only once
  if (write_readback_test_done != 0U) {
    return;
  }

  // Mark test as done
  write_readback_test_done = 1U;

  // Format EEPROM area for deterministic test
  status = Ee_Format();
  if (status != EE_OK) {
    UartLog_Printf("[TEST2] format=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST2] FAIL\r\n");
    return;
  }

  // Write baud rate config
  status = Ee_Write(CFG_BAUD_RATE, 115200UL);
  if (status != EE_OK) {
    UartLog_Printf("[TEST2] write CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST2] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST2] write CFG_BAUD_RATE=115200 OK\r\n");

  // Read baud rate config
  status = Ee_Read(CFG_BAUD_RATE, &read_value);
  if (status != EE_OK) {
    UartLog_Printf("[TEST2] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST2] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST2] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_value);

  // Check read value
  if (read_value != 115200UL) {
    UartLog_Printf("[TEST2] value_check=NG expected=115200 actual=%lu\r\n",
                   (unsigned long)read_value);
    UartLog_Printf("[TEST2] FAIL\r\n");
    return;
  }

  // Print write offset after one record
  UartLog_Printf("[TEST2] write_offset=%lu\r\n", (unsigned long)Ee_GetWriteOffset());

  // Check write offset
  if (Ee_GetWriteOffset() != (EE_HEADER_SIZE + EE_RECORD_SIZE)) {
    UartLog_Printf("[TEST2] write_offset_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)(EE_HEADER_SIZE + EE_RECORD_SIZE),
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST2] FAIL\r\n");
    return;
  }

  // Print test result
  UartLog_Printf("[TEST2] PASS\r\n");
}

// Print message for unimplemented test modes
static void App_RunNotImplemented(void)
{
  // Prevent repeated log output
  if (not_implemented_log_done != 0U) {
    return;
  }

  // Mark log as printed
  not_implemented_log_done = 1U;

  // Print selected test mode
  UartLog_Printf("[APP] Test mode not implemented yet: %lu (%s)\r\n",
                 (unsigned long)APP_TEST_MODE,
                 App_GetTestModeName(APP_TEST_MODE));
}

// Toggle LED periodically
static void App_UpdateHeartbeat(void)
{
  static uint32_t last_tick = 0U;
  uint32_t now = HAL_GetTick();

  // Check heartbeat timing
  if ((now - last_tick) >= APP_HEARTBEAT_PERIOD) {
    // Update tick
    last_tick = now;

    // Toggle LED
    HAL_GPIO_TogglePin(APP_LED_GPIO_Port, APP_LED_Pin);
  }
}

/* Function definitions ------------------------------------------------------*/

// Initialize application layer
void App_Init(UART_HandleTypeDef *debug_uart)
{
  // Initialize UART log
  UartLog_Init(debug_uart);
}

// Run application process
void App_Run(void)
{
    // Print boot log
  App_PrintBootLog();

  // Run selected test mode
#if APP_TEST_MODE == APP_TEST_MODE_BOOT_CHECK
  App_RunBootCheck();
#elif APP_TEST_MODE == APP_TEST_MODE_FORMAT_DEFAULT
  App_RunFormatDefault();
#elif APP_TEST_MODE == APP_TEST_MODE_WRITE_READBACK
  App_RunWriteReadback();
#else
  App_RunNotImplemented();
#endif

  // Update heartbeat
  App_UpdateHeartbeat();
}
