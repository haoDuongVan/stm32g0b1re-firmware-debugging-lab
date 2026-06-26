/*
 * uart_log.c
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "uart_log.h"

#include <stdarg.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/

#define UART_LOG_FORMAT_BUFFER_SIZE      0x100U  // 256 bytes for one formatted line
#define UART_LOG_TX_TIMEOUT              100U    // ms

/* Private variables ---------------------------------------------------------*/

static UART_HandleTypeDef *log_uart = NULL;
static osMutexId_t log_mutex = NULL;

/* Function definitions ------------------------------------------------------*/

// Store the UART handle and mutex used for all log output
void UartLog_Init(UART_HandleTypeDef *huart, osMutexId_t mutex_id)
{
  log_uart = huart;
  log_mutex = mutex_id;
}

/*
 * Mutex-protected UART printf for low-rate debug logs.
 *
 * This project is not the advanced non-blocking logger project.
 * Here, UART output is intentionally limited to low-rate MonitorTask logs and
 * occasional WorkerTask logs, so HAL_UART_Transmit() is acceptable.
 *
 * The mutex is used only after the RTOS scheduler is running. Boot logs are
 * printed before osKernelStart(), so acquiring a mutex at that time is avoided.
 */
// Print formatted log message; acquires mutex when RTOS is running
void UartLog_Printf(const char *format, ...)
{
  char format_buffer[UART_LOG_FORMAT_BUFFER_SIZE];
  va_list args;
  int len;

  if ((log_uart == NULL) || (format == NULL)) {
    return;
  }

  // Format the message into a local stack buffer
  va_start(args, format);
  len = vsnprintf(format_buffer, sizeof(format_buffer), format, args);
  va_end(args);

  // vsnprintf returns a negative value on encoding errors
  if (len <= 0) {
    return;
  }

  /*
   * vsnprintf returns the number of characters that *would* have been written,
   * even when truncated. Clamp to the actual buffer size so only valid bytes
   * are transmitted.
   */
  if ((uint32_t)len >= sizeof(format_buffer)) {
    len = (int)(sizeof(format_buffer) - 1U);
    format_buffer[len] = '\0';
  }

  // Acquire mutex only after the scheduler is running; boot logs skip it
  if ((log_mutex != NULL) && (osKernelGetState() == osKernelRunning)) {
    if (osMutexAcquire(log_mutex, osWaitForever) != osOK) {
      return;
    }

    (void)HAL_UART_Transmit(log_uart, (uint8_t *)format_buffer, (uint16_t)len, UART_LOG_TX_TIMEOUT);

    (void)osMutexRelease(log_mutex);
  } else {
    (void)HAL_UART_Transmit(log_uart, (uint8_t *)format_buffer, (uint16_t)len, UART_LOG_TX_TIMEOUT);
  }
}
