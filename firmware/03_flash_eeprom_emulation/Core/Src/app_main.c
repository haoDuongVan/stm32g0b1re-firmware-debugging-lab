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
#define APP_TEST_MODE_REBOOT_READBACK         4
#define APP_TEST_MODE_PAGE_TRANSFER           5
#define APP_TEST_MODE_PAGE_TRANSFER_REBOOT    6
#define APP_TEST_MODE_FAULT_AFTER_RECEIVE     7
#define APP_TEST_MODE_FAULT_AFTER_COPY        8
#define APP_TEST_MODE_CORRUPT_RECORD          9
#define APP_TEST_MODE_FAULT_AFTER_PROGRAM     10

// Set default test mode
#define APP_TEST_MODE                         APP_TEST_MODE_PAGE_TRANSFER_REBOOT


// Define reboot readback test phase values
#define APP_TEST_PHASE_AFTER_RESET            1U
#define APP_TEST_PHASE_DONE                   2U

// Define reboot readback test value
#define APP_TEST_REBOOT_BAUD_RATE             230400UL

// Define page transfer reboot test phase values
#define APP_TEST6_PHASE_AFTER_RESET           0x00000601UL
#define APP_TEST6_PHASE_DONE                  0x00000602UL

// Define page transfer reboot test values
#define APP_TEST6_FINAL_BAUD                  230400UL
#define APP_TEST6_TIMEOUT                     1000UL
#define APP_TEST6_MODE                        3UL

/*
 * TEST6 writes TEST_PHASE + TIMEOUT + MODE first.
 * Therefore baud fill count must leave exactly one slot for trigger write.
 */
#define APP_TEST6_FILL_COUNT                  (EE_RECORDS_PER_PAGE - 3U)

// Define page transfer test values
#define APP_TEST_PAGE_TRANSFER_FILL_COUNT     (EE_RECORDS_PER_PAGE - 2U)
#define APP_TEST_PAGE_TRANSFER_FINAL_BAUD     230400UL
#define APP_TEST_PAGE_TRANSFER_TIMEOUT        1000UL
#define APP_TEST_PAGE_TRANSFER_MODE           3UL

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
static uint8_t append_latest_test_done = 0U;
static uint8_t reboot_readback_test_done = 0U;
static uint8_t page_transfer_test_done = 0U;
static uint8_t page_transfer_reboot_test_done = 0U;
static uint8_t not_implemented_log_done = 0U;

/* Private function prototypes -----------------------------------------------*/
static const char *App_GetTestModeName(uint32_t test_mode);
static void App_PrintBootLog(void);
static void App_RunBootCheck(void);
static void App_RunFormatDefault(void);
static void App_RunWriteReadback(void);
static void App_RunAppendLatest(void);
static void App_RunRebootReadback(void);
static void App_RunPageTransfer(void);
static void App_RunPageTransferReboot(void);
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

    case APP_TEST_MODE_REBOOT_READBACK:
      return "reboot_readback";

    case APP_TEST_MODE_PAGE_TRANSFER:
      return "page_transfer";

    case APP_TEST_MODE_PAGE_TRANSFER_REBOOT:
      return "page_transfer_reboot";

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

// Run append latest value test
static void App_RunAppendLatest(void)
{
  EeStatus_t status;
  uint32_t read_value = 0U;
  uint32_t record_count = 0U;

  // Run this test only once
  if (append_latest_test_done != 0U) {
    return;
  }

  // Mark test as done
  append_latest_test_done = 1U;

  // Format EEPROM area for deterministic test
  status = Ee_Format();
  if (status != EE_OK) {
    UartLog_Printf("[TEST3] format=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }

  // Write first baud rate value
  status = Ee_Write(CFG_BAUD_RATE, 9600UL);
  if (status != EE_OK) {
    UartLog_Printf("[TEST3] write CFG_BAUD_RATE=9600 NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST3] write CFG_BAUD_RATE=9600 OK\r\n");

  // Write second baud rate value
  status = Ee_Write(CFG_BAUD_RATE, 115200UL);
  if (status != EE_OK) {
    UartLog_Printf("[TEST3] write CFG_BAUD_RATE=115200 NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST3] write CFG_BAUD_RATE=115200 OK\r\n");

  // Write latest baud rate value
  status = Ee_Write(CFG_BAUD_RATE, 230400UL);
  if (status != EE_OK) {
    UartLog_Printf("[TEST3] write CFG_BAUD_RATE=230400 NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST3] write CFG_BAUD_RATE=230400 OK\r\n");

  // Read latest baud rate value
  status = Ee_Read(CFG_BAUD_RATE, &read_value);
  if (status != EE_OK) {
    UartLog_Printf("[TEST3] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST3] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_value);

  // Check latest value
  if (read_value != 230400UL) {
    UartLog_Printf("[TEST3] latest_value_check=NG expected=230400 actual=%lu\r\n",
                   (unsigned long)read_value);
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }

  // Count valid records for CFG_BAUD_RATE
  record_count = Ee_CountRecordsForVar(CFG_BAUD_RATE);
  UartLog_Printf("[TEST3] record_count=%lu\r\n", (unsigned long)record_count);

  // Check record count
  if (record_count != 3U) {
    UartLog_Printf("[TEST3] record_count_check=NG expected=3 actual=%lu\r\n",
                   (unsigned long)record_count);
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }

  // Print write offset after 3 records
  UartLog_Printf("[TEST3] write_offset=%lu\r\n", (unsigned long)Ee_GetWriteOffset());

  // Check write offset
  if (Ee_GetWriteOffset() != (EE_HEADER_SIZE + (3U * EE_RECORD_SIZE))) {
    UartLog_Printf("[TEST3] write_offset_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)(EE_HEADER_SIZE + (3U * EE_RECORD_SIZE)),
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST3] FAIL\r\n");
    return;
  }

  // Print test result
  UartLog_Printf("[TEST3] PASS\r\n");
}

// Run reboot readback test
static void App_RunRebootReadback(void)
{
  EeStatus_t status;
  uint32_t phase = 0U;
  uint32_t read_value = 0U;

  // Run this test only once per boot
  if (reboot_readback_test_done != 0U) {
    return;
  }

  // Mark test as done for this boot
  reboot_readback_test_done = 1U;

  // Initialize EEPROM module from current Flash state
  status = Ee_Init();
  if (status != EE_OK) {
    UartLog_Printf("[TEST4] init=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST4] FAIL\r\n");
    return;
  }

  // Read test phase from Flash
  status = Ee_Read(EE_VAR_ID_TEST_PHASE, &phase);

  // If test was already completed, only verify value again
  if ((status == EE_OK) && (phase == APP_TEST_PHASE_DONE)) {
    UartLog_Printf("[TEST4] phase=done\r\n");

    // Read baud rate value
    status = Ee_Read(CFG_BAUD_RATE, &read_value);
    if (status != EE_OK) {
      UartLog_Printf("[TEST4] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST4] FAIL\r\n");
      return;
    }

    // Print read value
    UartLog_Printf("[TEST4] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_value);

    // Check read value
    if (read_value != APP_TEST_REBOOT_BAUD_RATE) {
      UartLog_Printf("[TEST4] value_check=NG expected=%lu actual=%lu\r\n",
                     (unsigned long)APP_TEST_REBOOT_BAUD_RATE,
                     (unsigned long)read_value);
      UartLog_Printf("[TEST4] FAIL\r\n");
      return;
    }

    // Print test result
    UartLog_Printf("[TEST4] PASS\r\n");
    return;
  }

  // Prepare test if phase is not found or not after-reset phase
  if ((status != EE_OK) || (phase != APP_TEST_PHASE_AFTER_RESET)) {
    UartLog_Printf("[TEST4] phase=prepare\r\n");

    // Format EEPROM area for deterministic test
    status = Ee_Format();
    if (status != EE_OK) {
      UartLog_Printf("[TEST4] format=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST4] FAIL\r\n");
      return;
    }

    // Write phase marker
    status = Ee_Write(EE_VAR_ID_TEST_PHASE, APP_TEST_PHASE_AFTER_RESET);
    if (status != EE_OK) {
      UartLog_Printf("[TEST4] write TEST_PHASE=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST4] FAIL\r\n");
      return;
    }
    UartLog_Printf("[TEST4] write TEST_PHASE=after_reset OK\r\n");

    // Write baud rate value
    status = Ee_Write(CFG_BAUD_RATE, APP_TEST_REBOOT_BAUD_RATE);
    if (status != EE_OK) {
      UartLog_Printf("[TEST4] write CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST4] FAIL\r\n");
      return;
    }
    UartLog_Printf("[TEST4] write CFG_BAUD_RATE=%lu OK\r\n",
                   (unsigned long)APP_TEST_REBOOT_BAUD_RATE);

    // Print write offset before reset
    UartLog_Printf("[TEST4] write_offset_before_reset=%lu\r\n",
                   (unsigned long)Ee_GetWriteOffset());

    // Trigger software reset
    UartLog_Printf("[TEST4] trigger software reset\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();

    return;
  }

  // Continue test after software reset
  UartLog_Printf("[TEST4] phase=after_reset\r\n");

  // Print restored runtime state
  UartLog_Printf("[TEST4] active_page=0x%08lX\r\n", (unsigned long)Ee_GetActivePageAddr());
  UartLog_Printf("[TEST4] write_offset_after_reset=%lu\r\n", (unsigned long)Ee_GetWriteOffset());

  // Check restored active page
  if (Ee_GetActivePageAddr() != EE_PAGE_A_ADDR) {
    UartLog_Printf("[TEST4] active_page_check=NG expected=0x%08lX actual=0x%08lX\r\n",
                   (unsigned long)EE_PAGE_A_ADDR,
                   (unsigned long)Ee_GetActivePageAddr());
    UartLog_Printf("[TEST4] FAIL\r\n");
    return;
  }

  // Check restored write offset
  if (Ee_GetWriteOffset() != (EE_HEADER_SIZE + (2U * EE_RECORD_SIZE))) {
    UartLog_Printf("[TEST4] write_offset_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)(EE_HEADER_SIZE + (2U * EE_RECORD_SIZE)),
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST4] FAIL\r\n");
    return;
  }

  // Read baud rate value after reset
  status = Ee_Read(CFG_BAUD_RATE, &read_value);
  if (status != EE_OK) {
    UartLog_Printf("[TEST4] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST4] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST4] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_value);

  // Check read value
  if (read_value != APP_TEST_REBOOT_BAUD_RATE) {
    UartLog_Printf("[TEST4] value_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)APP_TEST_REBOOT_BAUD_RATE,
                   (unsigned long)read_value);
    UartLog_Printf("[TEST4] FAIL\r\n");
    return;
  }

  // Mark test as completed to avoid reset loop on later boots
  status = Ee_Write(EE_VAR_ID_TEST_PHASE, APP_TEST_PHASE_DONE);
  if (status != EE_OK) {
    UartLog_Printf("[TEST4] write TEST_PHASE=done NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST4] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST4] write TEST_PHASE=done OK\r\n");

  // Print test result
  UartLog_Printf("[TEST4] PASS\r\n");
}

// Run page transfer test
static void App_RunPageTransfer(void)
{
  EeStatus_t status;
  uint32_t read_baud = 0U;
  uint32_t read_timeout = 0U;
  uint32_t read_mode = 0U;
  uint32_t page_a_count = 0U;
  uint32_t page_b_count = 0U;

  // Run this test only once
  if (page_transfer_test_done != 0U) {
    return;
  }

  // Mark test as done
  page_transfer_test_done = 1U;

  // Format EEPROM area for deterministic test
  status = Ee_Format();
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] format=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Write timeout config
  status = Ee_Write(CFG_TIMEOUT_MS, APP_TEST_PAGE_TRANSFER_TIMEOUT);
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] write CFG_TIMEOUT_MS=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Write mode config
  status = Ee_Write(CFG_MODE, APP_TEST_PAGE_TRANSFER_MODE);
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] write CFG_MODE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Print seed config result
  UartLog_Printf("[TEST5] write seed configs OK\r\n");

  // Fill active page with repeated baud rate records
  for (uint32_t idx = 0U; idx < APP_TEST_PAGE_TRANSFER_FILL_COUNT; idx++) {
    status = Ee_Write(CFG_BAUD_RATE, 9600UL + idx);
    if (status != EE_OK) {
      UartLog_Printf("[TEST5] fill failed idx=%lu status=%s\r\n",
                     (unsigned long)idx,
                     Ee_StatusToString(status));
      UartLog_Printf("[TEST5] FAIL\r\n");
      return;
    }
  }

  // Print fill result
  UartLog_Printf("[TEST5] fill_records=%lu OK\r\n",
                 (unsigned long)APP_TEST_PAGE_TRANSFER_FILL_COUNT);
  UartLog_Printf("[TEST5] write_offset_before_transfer=%lu\r\n",
                 (unsigned long)Ee_GetWriteOffset());

  // Check active page is full before transfer
  if (Ee_GetWriteOffset() != EE_PAGE_SIZE) {
    UartLog_Printf("[TEST5] full_page_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)EE_PAGE_SIZE,
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Write one more record to trigger page transfer
  status = Ee_Write(CFG_BAUD_RATE, APP_TEST_PAGE_TRANSFER_FINAL_BAUD);
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] trigger write=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST5] write final CFG_BAUD_RATE=%lu OK\r\n",
                 (unsigned long)APP_TEST_PAGE_TRANSFER_FINAL_BAUD);

  // Print active page and write offset after transfer
  UartLog_Printf("[TEST5] active_page_after_transfer=0x%08lX\r\n",
                 (unsigned long)Ee_GetActivePageAddr());
  UartLog_Printf("[TEST5] write_offset_after_transfer=%lu\r\n",
                 (unsigned long)Ee_GetWriteOffset());

  // Check active page after transfer
  if (Ee_GetActivePageAddr() != EE_PAGE_B_ADDR) {
    UartLog_Printf("[TEST5] active_page_check=NG expected=0x%08lX actual=0x%08lX\r\n",
                   (unsigned long)EE_PAGE_B_ADDR,
                   (unsigned long)Ee_GetActivePageAddr());
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Check write offset after transfer
  if (Ee_GetWriteOffset() != (EE_HEADER_SIZE + (3U * EE_RECORD_SIZE))) {
    UartLog_Printf("[TEST5] write_offset_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)(EE_HEADER_SIZE + (3U * EE_RECORD_SIZE)),
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Read baud rate after transfer
  status = Ee_Read(CFG_BAUD_RATE, &read_baud);
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Read timeout after transfer
  status = Ee_Read(CFG_TIMEOUT_MS, &read_timeout);
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] read CFG_TIMEOUT_MS=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Read mode after transfer
  status = Ee_Read(CFG_MODE, &read_mode);
  if (status != EE_OK) {
    UartLog_Printf("[TEST5] read CFG_MODE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Print read values
  UartLog_Printf("[TEST5] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_baud);
  UartLog_Printf("[TEST5] read CFG_TIMEOUT_MS=%lu OK\r\n", (unsigned long)read_timeout);
  UartLog_Printf("[TEST5] read CFG_MODE=%lu OK\r\n", (unsigned long)read_mode);

  // Check read values
  if ((read_baud != APP_TEST_PAGE_TRANSFER_FINAL_BAUD) ||
      (read_timeout != APP_TEST_PAGE_TRANSFER_TIMEOUT) ||
      (read_mode != APP_TEST_PAGE_TRANSFER_MODE)) {
    UartLog_Printf("[TEST5] value_check=NG\r\n");
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Count records in both pages
  page_a_count = Ee_CountValidRecords(EE_PAGE_A_ADDR);
  page_b_count = Ee_CountValidRecords(EE_PAGE_B_ADDR);

  // Print page states and record counts
  UartLog_Printf("[TEST5] page A state=%s valid_count=%lu\r\n",
                 Ee_PageStateToString(Ee_GetPageState(EE_PAGE_A_ADDR)),
                 (unsigned long)page_a_count);
  UartLog_Printf("[TEST5] page B state=%s valid_count=%lu\r\n",
                 Ee_PageStateToString(Ee_GetPageState(EE_PAGE_B_ADDR)),
                 (unsigned long)page_b_count);

  // Check final page states
  if ((Ee_GetPageState(EE_PAGE_A_ADDR) != EE_PAGE_ERASED) ||
      (Ee_GetPageState(EE_PAGE_B_ADDR) != EE_PAGE_ACTIVE)) {
    UartLog_Printf("[TEST5] page_state_check=NG\r\n");
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Check valid record count in new active page
  if (page_b_count != 3U) {
    UartLog_Printf("[TEST5] page_b_count_check=NG expected=3 actual=%lu\r\n",
                   (unsigned long)page_b_count);
    UartLog_Printf("[TEST5] FAIL\r\n");
    return;
  }

  // Print test result
  UartLog_Printf("[TEST5] PASS\r\n");
}

// Run page transfer reboot test
static void App_RunPageTransferReboot(void)
{
  EeStatus_t status;
  uint32_t phase = 0U;
  uint32_t read_baud = 0U;
  uint32_t read_timeout = 0U;
  uint32_t read_mode = 0U;

  // Run this test only once per boot
  if (page_transfer_reboot_test_done != 0U) {
    return;
  }

  // Mark test as done for this boot
  page_transfer_reboot_test_done = 1U;

  // Initialize EEPROM module from current Flash state
  status = Ee_Init();
  if (status != EE_OK) {
    UartLog_Printf("[TEST6] init=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Read test phase
  status = Ee_Read(EE_VAR_ID_TEST_PHASE, &phase);

  // Verify completed test state on later boots
  if ((status == EE_OK) && (phase == APP_TEST6_PHASE_DONE)) {
    UartLog_Printf("[TEST6] phase=done\r\n");

    // Read baud rate after completed state
    status = Ee_Read(CFG_BAUD_RATE, &read_baud);
    if (status != EE_OK) {
      UartLog_Printf("[TEST6] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }

    // Check baud rate value
    if (read_baud != APP_TEST6_FINAL_BAUD) {
      UartLog_Printf("[TEST6] value_check=NG expected=%lu actual=%lu\r\n",
                     (unsigned long)APP_TEST6_FINAL_BAUD,
                     (unsigned long)read_baud);
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }

    // Print completed test result
    UartLog_Printf("[TEST6] active_page=0x%08lX\r\n", (unsigned long)Ee_GetActivePageAddr());
    UartLog_Printf("[TEST6] write_offset=%lu\r\n", (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST6] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_baud);
    UartLog_Printf("[TEST6] PASS\r\n");
    return;
  }

  // Prepare page transfer when phase is not after-reset
  if ((status != EE_OK) || (phase != APP_TEST6_PHASE_AFTER_RESET)) {
    UartLog_Printf("[TEST6] phase=prepare\r\n");

    // Format EEPROM area for deterministic test
    status = Ee_Format();
    if (status != EE_OK) {
      UartLog_Printf("[TEST6] format=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }

    // Write phase marker before transfer
    status = Ee_Write(EE_VAR_ID_TEST_PHASE, APP_TEST6_PHASE_AFTER_RESET);
    if (status != EE_OK) {
      UartLog_Printf("[TEST6] write TEST_PHASE=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }
    UartLog_Printf("[TEST6] write TEST_PHASE=after_reset OK\r\n");

    // Write timeout config
    status = Ee_Write(CFG_TIMEOUT_MS, APP_TEST6_TIMEOUT);
    if (status != EE_OK) {
      UartLog_Printf("[TEST6] write CFG_TIMEOUT_MS=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }

    // Write mode config
    status = Ee_Write(CFG_MODE, APP_TEST6_MODE);
    if (status != EE_OK) {
      UartLog_Printf("[TEST6] write CFG_MODE=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }

    // Print seed config result
    UartLog_Printf("[TEST6] write seed configs OK\r\n");

    // Fill active page with repeated baud rate records
    for (uint32_t idx = 0U; idx < APP_TEST6_FILL_COUNT; idx++) {
      status = Ee_Write(CFG_BAUD_RATE, 9600UL + idx);
      if (status != EE_OK) {
        UartLog_Printf("[TEST6] fill failed idx=%lu status=%s\r\n",
                       (unsigned long)idx,
                       Ee_StatusToString(status));
        UartLog_Printf("[TEST6] FAIL\r\n");
        return;
      }
    }

    // Print fill result
    UartLog_Printf("[TEST6] fill_records=%lu OK\r\n", (unsigned long)APP_TEST6_FILL_COUNT);
    UartLog_Printf("[TEST6] write_offset_before_transfer=%lu\r\n",
                   (unsigned long)Ee_GetWriteOffset());

    // Check active page is full before transfer
    if (Ee_GetWriteOffset() != EE_PAGE_SIZE) {
      UartLog_Printf("[TEST6] full_page_check=NG expected=%lu actual=%lu\r\n",
                     (unsigned long)EE_PAGE_SIZE,
                     (unsigned long)Ee_GetWriteOffset());
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }

    // Write final baud rate to trigger page transfer
    status = Ee_Write(CFG_BAUD_RATE, APP_TEST6_FINAL_BAUD);
    if (status != EE_OK) {
      UartLog_Printf("[TEST6] trigger write=NG status=%s\r\n", Ee_StatusToString(status));
      UartLog_Printf("[TEST6] FAIL\r\n");
      return;
    }
    UartLog_Printf("[TEST6] write final CFG_BAUD_RATE=%lu OK\r\n",
                   (unsigned long)APP_TEST6_FINAL_BAUD);

    // Print state before reset
    UartLog_Printf("[TEST6] active_page_before_reset=0x%08lX\r\n",
                   (unsigned long)Ee_GetActivePageAddr());
    UartLog_Printf("[TEST6] write_offset_before_reset=%lu\r\n",
                   (unsigned long)Ee_GetWriteOffset());

    // Trigger software reset
    UartLog_Printf("[TEST6] trigger software reset\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();

    return;
  }

  // Continue test after software reset
  UartLog_Printf("[TEST6] phase=after_reset\r\n");

  // Print restored runtime state
  UartLog_Printf("[TEST6] active_page_after_reset=0x%08lX\r\n",
                 (unsigned long)Ee_GetActivePageAddr());
  UartLog_Printf("[TEST6] write_offset_after_reset=%lu\r\n",
                 (unsigned long)Ee_GetWriteOffset());

  // Check restored active page
  if (Ee_GetActivePageAddr() != EE_PAGE_B_ADDR) {
    UartLog_Printf("[TEST6] active_page_check=NG expected=0x%08lX actual=0x%08lX\r\n",
                   (unsigned long)EE_PAGE_B_ADDR,
                   (unsigned long)Ee_GetActivePageAddr());
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Check restored write offset
  if (Ee_GetWriteOffset() != (EE_HEADER_SIZE + (4U * EE_RECORD_SIZE))) {
    UartLog_Printf("[TEST6] write_offset_check=NG expected=%lu actual=%lu\r\n",
                   (unsigned long)(EE_HEADER_SIZE + (4U * EE_RECORD_SIZE)),
                   (unsigned long)Ee_GetWriteOffset());
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Read baud rate after reset
  status = Ee_Read(CFG_BAUD_RATE, &read_baud);
  if (status != EE_OK) {
    UartLog_Printf("[TEST6] read CFG_BAUD_RATE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Read timeout after reset
  status = Ee_Read(CFG_TIMEOUT_MS, &read_timeout);
  if (status != EE_OK) {
    UartLog_Printf("[TEST6] read CFG_TIMEOUT_MS=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Read mode after reset
  status = Ee_Read(CFG_MODE, &read_mode);
  if (status != EE_OK) {
    UartLog_Printf("[TEST6] read CFG_MODE=NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Print read values
  UartLog_Printf("[TEST6] read CFG_BAUD_RATE=%lu OK\r\n", (unsigned long)read_baud);
  UartLog_Printf("[TEST6] read CFG_TIMEOUT_MS=%lu OK\r\n", (unsigned long)read_timeout);
  UartLog_Printf("[TEST6] read CFG_MODE=%lu OK\r\n", (unsigned long)read_mode);

  // Check read values
  if ((read_baud != APP_TEST6_FINAL_BAUD) ||
      (read_timeout != APP_TEST6_TIMEOUT) ||
      (read_mode != APP_TEST6_MODE)) {
    UartLog_Printf("[TEST6] value_check=NG\r\n");
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }

  // Mark test as completed
  status = Ee_Write(EE_VAR_ID_TEST_PHASE, APP_TEST6_PHASE_DONE);
  if (status != EE_OK) {
    UartLog_Printf("[TEST6] write TEST_PHASE=done NG status=%s\r\n", Ee_StatusToString(status));
    UartLog_Printf("[TEST6] FAIL\r\n");
    return;
  }
  UartLog_Printf("[TEST6] write TEST_PHASE=done OK\r\n");

  // Print final page states
  UartLog_Printf("[TEST6] page A state=%s\r\n", Ee_PageStateToString(Ee_GetPageState(EE_PAGE_A_ADDR)));
  UartLog_Printf("[TEST6] page B state=%s\r\n", Ee_PageStateToString(Ee_GetPageState(EE_PAGE_B_ADDR)));

  // Print test result
  UartLog_Printf("[TEST6] PASS\r\n");
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
#elif APP_TEST_MODE == APP_TEST_MODE_APPEND_LATEST
  App_RunAppendLatest();
#elif APP_TEST_MODE == APP_TEST_MODE_REBOOT_READBACK
  App_RunRebootReadback();
#elif APP_TEST_MODE == APP_TEST_MODE_PAGE_TRANSFER
  App_RunPageTransfer();
#elif APP_TEST_MODE == APP_TEST_MODE_PAGE_TRANSFER_REBOOT
  App_RunPageTransferReboot();
#else
  App_RunNotImplemented();
#endif

  // Update heartbeat
  App_UpdateHeartbeat();
}
