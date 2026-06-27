/*
 * bl_update.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "bl_update.h"

#include "bl_flash_layout.h"
#include "bl_log.h"
#include "bl_slot.h"

/* Private defines -----------------------------------------------------------*/

/*
 * Time window during which the host can request update mode by sending 'u'.
 * After this window the bootloader proceeds to boot the selected slot.
 */
#define BL_UPDATE_WINDOW_MS              3000U

#define BL_UPDATE_ENTRY_CHAR_LOWER       'u'
#define BL_UPDATE_ENTRY_CHAR_UPPER       'U'

#define BL_UPDATE_CMD_MAX_LEN            32U
#define BL_UPDATE_RX_TIMEOUT_MS          100U

/*
 * Update mode entry test.
 */
#define BL_UPDATE_ENTRY_TEST_TAG         "[TEST14]"
#define BL_UPDATE_ENTRY_TEST_NAME        "uart_update_mode_entry_check"

/*
 * Basic UART command parser test.
 */
#define BL_UPDATE_CMD_TEST_TAG           "[TEST15]"
#define BL_UPDATE_CMD_TEST_NAME          "uart_command_parser_check"

/*
 * Slot erase command test.
 */
#define BL_UPDATE_ERASE_TEST_TAG         "[TEST16]"
#define BL_UPDATE_ERASE_TEST_NAME        "slot_erase_command_check"

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *update_uart = NULL;

/* Private function prototypes -----------------------------------------------*/
static void    BlUpdate_PrintPrompt(void);
static uint8_t BlUpdate_ReadLine(char *cmd, uint32_t max_len);
static char    BlUpdate_ToLower(char c);
static uint8_t BlUpdate_CommandEquals(const char *cmd, const char *expected);
static void    BlUpdate_HandleHelp(void);
static void    BlUpdate_HandleInfo(void);
static uint8_t BlUpdate_HandleCommand(const char *cmd);

/* Private functions ---------------------------------------------------------*/

// Print the interactive prompt
static void BlUpdate_PrintPrompt(void)
{
  BlLog_Printf("\r\nUPDATE> ");
}

// Read one line from UART into cmd; echoes each character; returns 1 when Enter received
static uint8_t BlUpdate_ReadLine(char *cmd, uint32_t max_len)
{
  uint32_t index;
  uint8_t rx_byte;
  HAL_StatusTypeDef status;

  if ((cmd == NULL) || (max_len == 0U)) {
    return 0U;
  }

  index = 0U;
  cmd[0] = '\0';

  while (1) {
    status = HAL_UART_Receive(update_uart, &rx_byte, 1U, BL_UPDATE_RX_TIMEOUT_MS);

    if (status != HAL_OK) {
      continue;
    }

    if ((rx_byte == '\r') || (rx_byte == '\n')) {
      cmd[index] = '\0';
      BlLog_Printf("\r\n");
      return 1U;
    }

    /*
     * Backspace (0x08) and DEL (0x7F): erase the last character if any.
     */
    if ((rx_byte == 0x08U) || (rx_byte == 0x7FU)) {
      if (index > 0U) {
        index--;
        cmd[index] = '\0';
        BlLog_Printf("\b \b");
      }
      continue;
    }

    // Accept printable ASCII only
    if ((rx_byte >= 0x20U) && (rx_byte <= 0x7EU)) {
      if (index < (max_len - 1U)) {
        cmd[index] = (char)rx_byte;
        index++;
        cmd[index] = '\0';
        BlLog_Printf("%c", rx_byte);
      }
    }
  }
}

// Convert a single ASCII character to lowercase
static char BlUpdate_ToLower(char c)
{
  if ((c >= 'A') && (c <= 'Z')) {
    return (char)(c - 'A' + 'a');
  }

  return c;
}

// Return 1 if cmd matches expected (case-insensitive, exact length)
static uint8_t BlUpdate_CommandEquals(const char *cmd, const char *expected)
{
  uint32_t i;

  if ((cmd == NULL) || (expected == NULL)) {
    return 0U;
  }

  i = 0U;

  while ((cmd[i] != '\0') && (expected[i] != '\0')) {
    if (BlUpdate_ToLower(cmd[i]) != expected[i]) {
      return 0U;
    }

    i++;
  }

  return ((cmd[i] == '\0') && (expected[i] == '\0')) ? 1U : 0U;
}

// Print the list of available commands
static void BlUpdate_HandleHelp(void)
{
  BlLog_Printf("[UPDATE] available_commands:\r\n");
  BlLog_Printf("[UPDATE]   help    - show command list\r\n");
  BlLog_Printf("[UPDATE]   info    - show bootloader and slot information\r\n");
  BlLog_Printf("[UPDATE]   erase b - erase Slot B\r\n");
  BlLog_Printf("[UPDATE]   exit    - leave update mode and boot normally\r\n");
  BlLog_Printf("[UPDATE]   reboot  - reset the MCU\r\n");
}

// Print bootloader and Flash layout information
static void BlUpdate_HandleInfo(void)
{
  BlLog_Printf("[UPDATE] bootloader_info:\r\n");
  BlLog_Printf("[UPDATE]   boot_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_BOOT_BASE_ADDR,
                (unsigned long)(BL_BOOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE]   slot_a_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_A_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE]   slot_b_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_B_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE]   metadata0=0x%08lX\r\n",
                (unsigned long)BL_METADATA0_BASE_ADDR);
  BlLog_Printf("[UPDATE]   metadata1=0x%08lX\r\n",
                (unsigned long)BL_METADATA1_BASE_ADDR);
}

/*
 * Dispatch one command string.
 * Return 1 to stay in update mode; return 0 to exit.
 */
static uint8_t BlUpdate_HandleCommand(const char *cmd)
{
  if ((cmd == NULL) || (cmd[0] == '\0')) {
    return 1U;
  }

  BlLog_Printf("[UPDATE] rx_command=%s\r\n", cmd);

  if (BlUpdate_CommandEquals(cmd, "help") != 0U) {
    BlUpdate_HandleHelp();
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "info") != 0U) {
    BlUpdate_HandleInfo();
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "erase b") != 0U) {
    uint8_t erase_result;

    BlLog_Printf("[UPDATE] erase_slot=B\r\n");
    BlLog_Printf("[UPDATE] erase_base=0x%08lX\r\n",
                  (unsigned long)BL_SLOT_B_BASE_ADDR);
    BlLog_Printf("[UPDATE] erase_size=%luKB\r\n",
                  (unsigned long)(BL_SLOT_SIZE / 1024UL));
    BlLog_Printf("[UPDATE] erase_start=YES\r\n");

    erase_result = BlSlot_Erase(BL_IMAGE_SLOT_B);

    if (erase_result != 0U) {
      BlLog_Printf("[UPDATE] erase_result=OK\r\n");
      BlLog_Printf("%s %s PASS\r\n",
                    BL_UPDATE_ERASE_TEST_TAG,
                    BL_UPDATE_ERASE_TEST_NAME);
    } else {
      BlLog_Printf("[UPDATE] erase_result=NG\r\n");
      BlLog_Printf("%s %s FAIL\r\n",
                    BL_UPDATE_ERASE_TEST_TAG,
                    BL_UPDATE_ERASE_TEST_NAME);
    }

    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "exit") != 0U) {
    BlLog_Printf("[UPDATE] exit_to_normal_boot=YES\r\n");
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    return 0U;
  }

  if (BlUpdate_CommandEquals(cmd, "reboot") != 0U) {
    BlLog_Printf("[UPDATE] reboot_request=YES\r\n");
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    HAL_Delay(100U);
    NVIC_SystemReset();
    /* never reached */
  }

  BlLog_Printf("[UPDATE] command_result=UNKNOWN\r\n");
  BlLog_Printf("[UPDATE] hint=type_help\r\n");

  return 1U;
}

/* Function definitions ------------------------------------------------------*/

// Store the UART handle used for all update communication
void BlUpdate_Init(UART_HandleTypeDef *uart)
{
  update_uart = uart;
}

// Poll UART for BL_UPDATE_WINDOW_MS; return 1 if 'u' or 'U' is received
uint8_t BlUpdate_WaitForEntry(void)
{
  uint32_t start_tick;
  uint32_t elapsed;
  uint8_t rx_byte;
  HAL_StatusTypeDef status;

  if (update_uart == NULL) {
    return 0U;
  }

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] uart_update_window=%ums\r\n",
                (unsigned int)BL_UPDATE_WINDOW_MS);
  BlLog_Printf("[BOOT] send_u_to_enter_update_mode\r\n");

  start_tick = HAL_GetTick();

  while (1) {
    elapsed = HAL_GetTick() - start_tick;

    if (elapsed >= BL_UPDATE_WINDOW_MS) {
      break;
    }

    /*
     * Poll with 1 ms timeout so elapsed time can be checked frequently.
     * HAL_UART_Receive returns HAL_OK only when a byte is received.
     */
    status = HAL_UART_Receive(update_uart, &rx_byte, 1U, 1U);

    if (status == HAL_OK) {
      if ((rx_byte == BL_UPDATE_ENTRY_CHAR_LOWER) ||
          (rx_byte == BL_UPDATE_ENTRY_CHAR_UPPER)) {
        BlLog_Printf("[BOOT] uart_update_request=YES\r\n");
        BlLog_Printf("[BOOT] update_mode=ENTER\r\n");
        BlLog_Printf("%s %s PASS\r\n",
                      BL_UPDATE_ENTRY_TEST_TAG,
                      BL_UPDATE_ENTRY_TEST_NAME);
        return 1U;
      }
    }
  }

  BlLog_Printf("[BOOT] uart_update_request=NO\r\n");
  BlLog_Printf("[BOOT] normal_boot=START\r\n");

  return 0U;
}

// Run the interactive update command loop; returns when 'exit' command is received
void BlUpdate_Run(void)
{
  char cmd[BL_UPDATE_CMD_MAX_LEN];

  BlLog_Printf("\r\n");
  BlLog_Printf("[UPDATE] command_mode=READY\r\n");
  BlLog_Printf("[UPDATE] waiting_command...\r\n");
  BlUpdate_HandleHelp();

  while (1) {
    BlUpdate_PrintPrompt();

    if (BlUpdate_ReadLine(cmd, sizeof(cmd)) == 0U) {
      continue;
    }

    if (BlUpdate_HandleCommand(cmd) == 0U) {
      BlLog_Printf("[UPDATE] command_mode=EXIT\r\n");
      return;
    }
  }
}
