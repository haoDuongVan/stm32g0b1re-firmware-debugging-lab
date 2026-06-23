/*
 * uart_logger.c
 *
 *  Created on: Jun 23, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "uart_logger.h"
#include "ring_buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define DEBUG_LOGGER_BUFFER_SIZE          0x800   // 2KB ring buffer for queued log data

#define DEBUG_LOGGER_FORMAT_BUFFER_SIZE   0x100   // 256 bytes for one formatted message

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *debug_uart = NULL;

// Backing storage for the ring buffer
static uint8_t logger_storage[DEBUG_LOGGER_BUFFER_SIZE];
static RingBuffer_t logger_rb;

/*
 * tx_busy and current_tx_len are shared between task context and the DMA TX
 * complete ISR, so they must be volatile and accessed only inside a critical
 * section.
 */
static volatile bool tx_busy = false;
static volatile uint16_t current_tx_len = 0U;
static volatile uint32_t dropped_count = 0U;

/* Private functions ---------------------------------------------------------*/
// Disable interrupts and return the previous PRIMASK value
static uint32_t DebugLogger_EnterCritical(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

// Restore interrupts to the state saved by DebugLogger_EnterCritical
static void DebugLogger_ExitCritical(uint32_t primask)
{
  // Only re-enable interrupts if they were enabled before entering critical section
  if (primask == 0) {
    __enable_irq();
  }
}

// Kick off the next DMA transfer if the TX channel is idle and data is ready
static void DebugLogger_StartNextTransfer(void)
{
  uint8_t *tx_data = 0;
  uint16_t tx_len = 0U;
  uint32_t primask;
  HAL_StatusTypeDef status;

  // Do nothing if the logger has not been initialized
  if (debug_uart == NULL)  {
    return;
  }

  primask = DebugLogger_EnterCritical();

  // Another DMA transfer is already in progress, bail out
  if (tx_busy) {
    DebugLogger_ExitCritical(primask);
    return;
  }

  // Peek at the contiguous data available starting from the ring buffer tail
  tx_len = RingBuffer_PeekContiguous(&logger_rb, &tx_data);

  // Nothing to send
  if ((tx_len == 0U) || (tx_data == 0)) {
    DebugLogger_ExitCritical(primask);
    return;
  }

  // Mark TX as busy before releasing the critical section to prevent re-entry
  tx_busy = true;
  current_tx_len = tx_len;

  DebugLogger_ExitCritical(primask);

  status = HAL_UART_Transmit_DMA(debug_uart, tx_data, tx_len);

  if (status != HAL_OK) {
    /*
     * DMA failed to start. The ring buffer tail is NOT advanced, so the data
     * is still in the buffer and will be retried on the next StartNextTransfer
     * call. Do not increment dropped_count — nothing was actually dropped.
     */
    primask = DebugLogger_EnterCritical();

    tx_busy = false;
    current_tx_len = 0U;

    DebugLogger_ExitCritical(primask);
  }
}

// Initializes the logger with the given UART handle and resets all state
void DebugLogger_Init(UART_HandleTypeDef *huart)
{
  uint32_t primask;

  primask = DebugLogger_EnterCritical();

  // Store the UART handle and initialize the ring buffer over the static storage
  debug_uart = huart;
  RingBuffer_Init(&logger_rb, logger_storage, (uint16_t)sizeof(logger_storage));

  // Reset TX state and counters
  tx_busy = false;
  current_tx_len = 0U;
  dropped_count = 0U;

  DebugLogger_ExitCritical(primask);
}

// Copies raw bytes into the ring buffer and starts a DMA transfer if idle
bool DebugLogger_Write(const uint8_t *data, uint16_t len)
{
  bool written;
  uint32_t primask;

  // Reject invalid arguments
  if ((data == 0) || (len == 0U)) {
      return false;
  }

  primask = DebugLogger_EnterCritical();

  // Drop the message atomically if there is not enough free space
  if (len > RingBuffer_GetFree(&logger_rb)) {
    dropped_count++;
    DebugLogger_ExitCritical(primask);
    return false;
  }

  written = RingBuffer_Write(&logger_rb, data, len);

  // RingBuffer_Write can still fail if the ring buffer state is inconsistent
  if (!written) {
    dropped_count++;
  }

  DebugLogger_ExitCritical(primask);

  // Start a transfer outside the critical section to keep ISR latency low
  if (written) {
    DebugLogger_StartNextTransfer();
  }

  return written;
}

// Formats a printf-style string and writes it to the ring buffer
void DebugLogger_Printf(const char *format, ...)
{
  char format_buffer[DEBUG_LOGGER_FORMAT_BUFFER_SIZE];
  va_list args;
  int len;

  // Reject NULL format string
  if (format == NULL) {
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
   * even when truncated.  Clamp to the actual buffer size so DebugLogger_Write
   * only sees valid bytes.
   */
  if ((uint32_t)len >= sizeof(format_buffer)) {
    len = (int)(sizeof(format_buffer) - 1U);
    format_buffer[len] = '\0';
  }

  (void)DebugLogger_Write((const uint8_t *)format_buffer, (uint16_t)len);
}

// Called from HAL_UART_TxCpltCallback; advances the ring buffer tail and starts the next transfer
void DebugLogger_TxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t completed_len;
  uint32_t primask;

  // Ignore callbacks from other UART peripherals
  if ((debug_uart == 0) || (huart != debug_uart)) {
    return;
  }

  primask = DebugLogger_EnterCritical();

  // Read the length that was handed to DMA before clearing the busy flag
  completed_len = current_tx_len;

  if (completed_len > 0U) {
    // Free the bytes that DMA just transmitted
    (void)RingBuffer_AdvanceTail(&logger_rb, completed_len);
  }

  current_tx_len = 0U;
  tx_busy = false;

  DebugLogger_ExitCritical(primask);

  // Chain the next transfer without waiting for the next DebugLogger_Write call
  DebugLogger_StartNextTransfer();
}

// Returns the total number of messages dropped due to buffer full or DMA errors
uint32_t DebugLogger_GetDroppedCount(void)
{
  return dropped_count;
}

// Returns the number of bytes currently waiting in the ring buffer to be transmitted
uint16_t DebugLogger_GetBufferedSize(void)
{
  uint16_t used;
  uint32_t primask;

  // Read under critical section because head/tail can be modified from ISR
  primask = DebugLogger_EnterCritical();
  used = RingBuffer_GetUsed(&logger_rb);
  DebugLogger_ExitCritical(primask);

  return used;
}
