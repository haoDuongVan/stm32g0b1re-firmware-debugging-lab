/*
 * app_log.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "app_log.h"

#include <stdarg.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define APP_LOG_FORMAT_BUFFER_SIZE       0x100U    /* 256 bytes */
#define APP_LOG_UART_TIMEOUT_MS          100U

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *debug_uart = NULL;

/* Function definitions ------------------------------------------------------*/

/**
 * @brief  Initialize application UART logger.
 * @param  debug_uart_handle UART handle used for debug output.
 * @retval None
 */
void AppLog_Init(UART_HandleTypeDef *debug_uart_handle)
{
  /*
   * Store the UART handle instead of using huart2 directly.
   * This keeps the logger independent from the generated peripheral name.
   */
  debug_uart = debug_uart_handle;
}

/**
 * @brief  Print formatted log message through UART.
 * @param  format printf-style format string.
 * @retval None
 */
void AppLog_Printf(const char *format, ...)
{
  char format_buffer[APP_LOG_FORMAT_BUFFER_SIZE];
  va_list args;
  int len;

  if (format == NULL) {
    return;
  }

  if (debug_uart == NULL) {
    return;
  }

  va_start(args, format);
  len = vsnprintf(format_buffer, sizeof(format_buffer), format, args);
  va_end(args);

  if (len <= 0) {
    return;
  }

  /*
   * vsnprintf returns the number of characters that would have been written.
   * If the output is truncated, clamp the transmit length to the actual buffer.
   */
  if ((uint32_t)len >= sizeof(format_buffer)) {
    len = (int)(sizeof(format_buffer) - 1U);
    format_buffer[len] = '\0';
  }

  (void)HAL_UART_Transmit(debug_uart,
                          (uint8_t *)format_buffer,
                          (uint16_t)len,
                          APP_LOG_UART_TIMEOUT_MS);
}