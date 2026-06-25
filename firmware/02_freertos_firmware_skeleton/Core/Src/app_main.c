/*
 * app_main.c
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "app_main.h"
#include "app_events.h"
#include "app_tasks.h"
#include "uart_log.h"

#include "cmsis_os.h"

/* Private variables ---------------------------------------------------------*/

static UART_HandleTypeDef *app_debug_uart = NULL;
static osMessageQueueId_t app_event_queue = NULL;
static osMutexId_t uart_log_mutex = NULL;

static const osMessageQueueAttr_t app_event_queue_attributes = {
  .name = "AppEventQueue"
};

static const osMutexAttr_t uart_log_mutex_attributes = {
  .name = "UartLogMutex"
};

/* Function definitions ------------------------------------------------------*/

/*
 * Store the UART handle used for debug logs.
 *
 * Call this from main.c after MX_USART2_UART_Init().
 * This avoids including usart.h and follows the same style as the previous
 * project, where the app layer receives peripheral handles from main.c.
 */
void App_Init(UART_HandleTypeDef *debug_uart)
{
  app_debug_uart = debug_uart;
}

/*
 * Create application RTOS objects.
 *
 * This function is called from main.c inside the USER CODE RTOS_THREADS block,
 * after osKernelInitialize() and before osKernelStart().
 */
void App_CreateRtosObjects(void)
{
  uart_log_mutex = osMutexNew(&uart_log_mutex_attributes);
  UartLog_Init(app_debug_uart, uart_log_mutex);

  app_event_queue = osMessageQueueNew(APP_EVENT_QUEUE_LENGTH,
                                      sizeof(AppEvent_t),
                                      &app_event_queue_attributes);

  UartLog_Printf("\r\n");
  UartLog_Printf("[BOOT] STM32G0B1RE Firmware Debugging Lab\r\n");
  UartLog_Printf("[BOOT] Project: 02_freertos_firmware_skeleton\r\n");
  UartLog_Printf("[BOOT] Board: NUCLEO-G0B1RE\r\n");
  UartLog_Printf("[BOOT] System clock: 48 MHz\r\n");

#if APP_TEST_MODE == APP_TEST_MODE_QUEUE_STRESS
  UartLog_Printf("[BOOT] Test mode: queue stress\r\n");
#else
  UartLog_Printf("[BOOT] Test mode: normal\r\n");
#endif

  if (app_debug_uart == NULL) {
    UartLog_Printf("[BOOT] ERROR: debug UART is NULL\r\n");
  }

  if (uart_log_mutex == NULL) {
    UartLog_Printf("[BOOT] ERROR: UartLogMutex create failed\r\n");
  }

  if (app_event_queue == NULL) {
    UartLog_Printf("[BOOT] ERROR: AppEventQueue create failed\r\n");
  }

  UartLog_Printf("[BOOT] FreeRTOS objects created\r\n");

  AppTasks_Init(app_event_queue);
  AppTasks_Create();

  UartLog_Printf("[BOOT] FreeRTOS kernel starting\r\n");
}
