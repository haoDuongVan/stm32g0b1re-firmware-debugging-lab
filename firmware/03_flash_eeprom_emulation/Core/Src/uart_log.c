/*
 * uart_log.c
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "uart_log.h"

#include <stdarg.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define UART_LOG_FORMAT_BUFFER_SIZE      0x100U   // 256 bytes
#define UART_LOG_TIMEOUT                 100U     // ms

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *debug_uart = NULL;

/* Function definitions ------------------------------------------------------*/

// Initialize UART log module
void UartLog_Init(UART_HandleTypeDef *debug_uart_handle)
{
  // Store UART handle
  debug_uart = debug_uart_handle;
}

// Print formatted log message through UART
void UartLog_Printf(const char *format, ...)
{
  char format_buffer[UART_LOG_FORMAT_BUFFER_SIZE];
  va_list args;
  int len;

  // Check format string
  if (format == NULL) {
    return;
  }

  // Check UART handle
  if (debug_uart == NULL) {
    return;
  }

  // Format message
  va_start(args, format);
  len = vsnprintf(format_buffer, sizeof(format_buffer), format, args);
  va_end(args);

  // Check formatting result
  if (len <= 0) {
    return;
  }

  // Clamp message length when truncated
  if ((uint32_t)len >= sizeof(format_buffer)) {
    len = (int)(sizeof(format_buffer) - 1U);
    format_buffer[len] = '\0';
  }

  // Send log by blocking UART transmit
  (void)HAL_UART_Transmit(debug_uart,
                          (uint8_t *)format_buffer,
                          (uint16_t)len,
                          UART_LOG_TIMEOUT);
}
