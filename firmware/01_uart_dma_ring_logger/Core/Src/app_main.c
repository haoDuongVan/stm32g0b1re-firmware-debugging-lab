/*
 * app_main.c
 *
 *  Created on: Jun 23, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "app_main.h"
#include "uart_logger.h"
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
/*
 * Create 4 test modes in this app.
 *
 * APP_TEST_MODE_NORMAL:
 *   Print one tick log every second.
 *
 * APP_TEST_MODE_SPAM:
 *   Print a short burst of logs once after boot.
 *
 * APP_TEST_MODE_BUFFER_FULL:
 *   Print a large burst of logs once after boot to force buffer full behavior.
 *
 * APP_TEST_MODE_BLOCKING_PRINTF:
 *   Print a large burst using HAL_UART_Transmit (blocking) to show that the
 *   CPU is stalled during transmission — [MAIN] tick logs and LED heartbeat
 *   stop firing until the burst completes.
 */
#define APP_TEST_MODE_NORMAL            0
#define APP_TEST_MODE_SPAM              1
#define APP_TEST_MODE_BUFFER_FULL       2
#define APP_TEST_MODE_BLOCKING_PRINTF   3

// Set default test mode to normal
#define APP_TEST_MODE               APP_TEST_MODE_NORMAL

/* 
 * Define the GPIO pin and port for the LED.
 */
#define APP_LED_GPIO_Port           GPIOA
#define APP_LED_Pin                 GPIO_PIN_5

/*
 * Define test timing constants.
 */
#define ONE_SECOND                  1000  //ms
#define TWO_SECONDS                 2000  //ms

/* Private variables ---------------------------------------------------------*/
#if APP_TEST_MODE == APP_TEST_MODE_SPAM
static uint8_t spam_test_done = 0;
#endif

#if APP_TEST_MODE == APP_TEST_MODE_BUFFER_FULL
static uint8_t buffer_full_test_done = 0;
#endif

#if APP_TEST_MODE == APP_TEST_MODE_BLOCKING_PRINTF
static UART_HandleTypeDef *blocking_uart = NULL;
static uint8_t blocking_test_done = 0;
#endif

/* Private functions ---------------------------------------------------------*/
#if APP_TEST_MODE == APP_TEST_MODE_SPAM
static void App_RunSpamTest(void)
{
  // Check and set spam test flag
  if (spam_test_done != 0U) {
    return;
  }

  spam_test_done = 1;

  // Run spam test
  DebugLogger_Printf("[TEST] spam log start\r\n");

  for (uint32_t i = 0; i < 100U; i++)
  {
    DebugLogger_Printf("[SPAM] index=%lu tick=%lu\r\n",
                        (unsigned long)i,
                        (unsigned long)HAL_GetTick());
  }

  DebugLogger_Printf("[TEST] spam log queued dropped=%lu buffered=%u\r\n",
                      (unsigned long)DebugLogger_GetDroppedCount(),
                      (unsigned int)DebugLogger_GetBufferedSize());
}
#endif

#if APP_TEST_MODE == APP_TEST_MODE_BLOCKING_PRINTF
static void App_RunBlockingPrintfTest(void)
{
  char buf[128];
  int len;

  // Check and set blocking test flag
  if (blocking_test_done != 0U) {
    return;
  }

  blocking_test_done = 1U;

  /*
   * Wait for the DMA logger to drain all boot messages before switching to
   * blocking transmit. Both share the same UART handle — calling
   * HAL_UART_Transmit while a DMA transfer is in progress returns HAL_BUSY.
   */
  HAL_Delay(500U);

  len = snprintf(buf, sizeof(buf), "[TEST] blocking printf test start tick=%lu\r\n",
                 (unsigned long)HAL_GetTick());
  if (len > 0) {
    HAL_UART_Transmit(blocking_uart, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
  }

  // CPU is stalled inside HAL_UART_Transmit for every single message
  for (uint32_t i = 0; i < 500U; i++)
  {
    len = snprintf(buf, sizeof(buf),
                   "[BLOCK] index=%lu tick=%lu payload=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\r\n",
                   (unsigned long)i,
                   (unsigned long)HAL_GetTick());
    if (len > 0) {
      HAL_UART_Transmit(blocking_uart, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
    }
  }

  /*
   * This line prints immediately after the loop — tick here reveals the total
   * wall-clock time the CPU was blocked inside HAL_UART_Transmit.
   */
  len = snprintf(buf, sizeof(buf), "[TEST] blocking printf test done tick=%lu\r\n",
                 (unsigned long)HAL_GetTick());
  if (len > 0) {
    HAL_UART_Transmit(blocking_uart, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
  }
}
#endif

#if APP_TEST_MODE == APP_TEST_MODE_BUFFER_FULL
static void App_RunBufferFullTest(void)
{
  // Check and set buffer full test flag
  if (buffer_full_test_done != 0U) {
    return;
  }

  buffer_full_test_done = 1U;

  // Run buffer full test
  DebugLogger_Printf("[TEST] buffer full test start\r\n");

  for (uint32_t i = 0; i < 500U; i++)
  {
    DebugLogger_Printf("[FILL] index=%lu tick=%lu payload=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\r\n",
                        (unsigned long)i,
                        (unsigned long)HAL_GetTick());
  }

  DebugLogger_Printf("[TEST] buffer full test queued dropped=%lu buffered=%u\r\n",
                      (unsigned long)DebugLogger_GetDroppedCount(),
                      (unsigned int)DebugLogger_GetBufferedSize());
}
#endif

void App_Init(UART_HandleTypeDef *debug_uart)
{
  DebugLogger_Init(debug_uart);

#if APP_TEST_MODE == APP_TEST_MODE_BLOCKING_PRINTF
  // Store the UART handle for use in the blocking test
  blocking_uart = debug_uart;
#endif

  DebugLogger_Printf("\r\n");
  DebugLogger_Printf("[BOOT] STM32G0B1RE Firmware Debugging Lab\r\n");
  DebugLogger_Printf("[BOOT] Project: 01_uart_dma_ring_logger\r\n");
  DebugLogger_Printf("[BOOT] Board: NUCLEO-G0B1RE\r\n");
  DebugLogger_Printf("[BOOT] System clock: 48 MHz\r\n");
  DebugLogger_Printf("[BOOT] UART DMA logger initialized\r\n");

#if APP_TEST_MODE == APP_TEST_MODE_NORMAL
  DebugLogger_Printf("[BOOT] Test mode: normal\r\n");
#elif APP_TEST_MODE == APP_TEST_MODE_SPAM
  DebugLogger_Printf("[BOOT] Test mode: spam\r\n");
#elif APP_TEST_MODE == APP_TEST_MODE_BUFFER_FULL
  DebugLogger_Printf("[BOOT] Test mode: buffer full\r\n");
#elif APP_TEST_MODE == APP_TEST_MODE_BLOCKING_PRINTF
  DebugLogger_Printf("[BOOT] Test mode: blocking printf\r\n");
#else
  DebugLogger_Printf("[BOOT] Test mode: unknown\r\n");
#endif
}

void App_Run(void)
{
  // Persists across calls to track when the last periodic log was printed
  static uint32_t last_tick = 0U;
  uint32_t now = HAL_GetTick();

  /*
   * Delay the one-shot tests by TWO_SECONDS so the boot log messages have
   * time to drain from the ring buffer before flooding it with test data.
   */
#if APP_TEST_MODE == APP_TEST_MODE_SPAM
  if (now >= TWO_SECONDS) {
    App_RunSpamTest();
  }
#elif APP_TEST_MODE == APP_TEST_MODE_BUFFER_FULL
  if (now >= TWO_SECONDS) {
    App_RunBufferFullTest();
  }
#elif APP_TEST_MODE == APP_TEST_MODE_BLOCKING_PRINTF
  if (now >= TWO_SECONDS) {
    App_RunBlockingPrintfTest();
    // Refresh now so the periodic tick check below uses real time after the blocking test
    now = HAL_GetTick();
  }
#endif

  // uint32_t subtraction is wrap-safe, so this works correctly after HAL_GetTick() overflows
  if ((now - last_tick) >= ONE_SECOND) {
    last_tick = now;

    // Toggle LED as a heartbeat indicator
    HAL_GPIO_TogglePin(APP_LED_GPIO_Port, APP_LED_Pin);

    DebugLogger_Printf("[MAIN] tick=%lu dropped=%lu buffered=%u\r\n",
                        (unsigned long)now,
                        (unsigned long)DebugLogger_GetDroppedCount(),
                        (unsigned int)DebugLogger_GetBufferedSize());
  }
}
