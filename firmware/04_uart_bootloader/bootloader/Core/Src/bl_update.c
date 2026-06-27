/*
 * bl_update.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "bl_update.h"

#include "bl_log.h"

/* Private defines -----------------------------------------------------------*/

/*
 * Time window during which the host can request update mode by sending 'u'.
 * After this window the bootloader proceeds to boot the selected slot.
 */
#define BL_UPDATE_WINDOW_MS              3000U

#define BL_UPDATE_ENTRY_CHAR_LOWER       'u'
#define BL_UPDATE_ENTRY_CHAR_UPPER       'U'

/*
 * Update mode entry test.
 */
#define BL_UPDATE_ENTRY_TEST_TAG         "[TEST14]"
#define BL_UPDATE_ENTRY_TEST_NAME        "uart_update_mode_entry_check"

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *update_uart = NULL;

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
     * Poll with 1 ms timeout so we check the elapsed time frequently.
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

// Update mode main loop; placeholder until command parser is implemented
void BlUpdate_Run(void)
{
  BlLog_Printf("\r\n");
  BlLog_Printf("[UPDATE] command_mode=READY\r\n");
  BlLog_Printf("[UPDATE] waiting_command...\r\n");

  /*
   * Milestone 14: command mode skeleton only.
   * The loop blocks here until Milestone 15 adds a command parser.
   */
  while (1) {
    HAL_Delay(100U);
  }
}
