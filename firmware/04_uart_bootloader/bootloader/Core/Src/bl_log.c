/*
 * bl_log.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "bl_log.h"

#include <stdarg.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define BL_LOG_FORMAT_BUFFER_SIZE        0x100U   // 256 bytes for one formatted message
#define BL_LOG_TIMEOUT                   100U     // ms

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *debug_uart = NULL;

/* Function definitions ------------------------------------------------------*/
void BlLog_Init(UART_HandleTypeDef *debug_uart_handle)
{
  // Store UART handle for bootloader debug logs
  debug_uart = debug_uart_handle;
}

void BlLog_Printf(const char *format, ...)
{
  char format_buffer[BL_LOG_FORMAT_BUFFER_SIZE];
  va_list args;
  int len;

  // Reject NULL format string
  if (format == NULL) {
    return;
  }

  // Do nothing if logger is not initialized
  if (debug_uart == NULL) {
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
   * vsnprintf returns the number of characters that would have been written,
   * even when truncated. Clamp to the actual buffer size before sending.
   */
  if ((uint32_t)len >= sizeof(format_buffer)) {
    len = (int)(sizeof(format_buffer) - 1U);
    format_buffer[len] = '\0';
  }

  (void)HAL_UART_Transmit(debug_uart,
                          (uint8_t *)format_buffer,
                          (uint16_t)len,
                          BL_LOG_TIMEOUT);
}