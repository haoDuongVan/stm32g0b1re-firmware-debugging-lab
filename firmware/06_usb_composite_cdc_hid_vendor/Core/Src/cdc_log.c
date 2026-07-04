/*
 * cdc_log.c
 *
 *  Created on: Jul 5, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "cdc_log.h"
#include "usbd_composite.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define CDC_LOG_BUF_MASK    (CDC_LOG_BUF_SIZE - 1U)
#define CDC_LOG_PRINTF_MAX  128U

/* Private variables ---------------------------------------------------------*/
static uint8_t  gRingBuf[CDC_LOG_BUF_SIZE];
static uint16_t gWriteIdx;
static uint16_t gReadIdx;

/*
 * Scratch buffer used by CdcLog_Run to hold one USB packet before handing
 * it to USBD_COMPOSITE_CDC_Transmit.  Kept static so the buffer remains
 * valid while the USB DMA transfer is in progress after the function returns.
 */
static uint8_t gTxScratch[COMP_CDC_DATA_EP_SIZE];

/* External variables --------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Private functions ---------------------------------------------------------*/

// Return the number of bytes currently stored in the ring buffer
static uint16_t RingBuf_Used(void)
{
  return (gWriteIdx - gReadIdx) & CDC_LOG_BUF_MASK;
}

static uint16_t RingBuf_Free(void)
{
  /* One slot is always kept empty to distinguish full from empty. */
  return (CDC_LOG_BUF_SIZE - 1U) - RingBuf_Used();
}

// Push one byte; caller must verify space is available first
static void RingBuf_Push(uint8_t byte)
{
  gRingBuf[gWriteIdx] = byte;
  gWriteIdx = (gWriteIdx + 1U) & CDC_LOG_BUF_MASK;
}

// Pop up to maxLen bytes into dst; returns the number of bytes actually popped
static uint16_t RingBuf_Pop(uint8_t *dst, uint16_t maxLen)
{
  uint16_t avail = RingBuf_Used();
  uint16_t count = (avail < maxLen) ? avail : maxLen;

  for (uint16_t i = 0U; i < count; i++)
  {
    dst[i]   = gRingBuf[gReadIdx];
    gReadIdx = (gReadIdx + 1U) & CDC_LOG_BUF_MASK;
  }

  return count;
}

/* Function definitions ------------------------------------------------------*/

// Reset the ring buffer to empty; call once before starting the main loop
void CdcLog_Init(void)
{
  gWriteIdx = 0U;
  gReadIdx  = 0U;
  (void)memset(gRingBuf, 0, sizeof(gRingBuf));
}

/*
 * Send one USB packet worth of buffered data to the CDC IN endpoint.
 * Returns immediately if the host has not opened the virtual COM port,
 * if a previous transfer is still pending, or if the buffer is empty.
 */
void CdcLog_Run(void)
{
  if (!USBD_COMPOSITE_CDC_IsHostConnected(&hUsbDeviceFS))
  {
    return;
  }

  if (!USBD_COMPOSITE_CDC_IsTxIdle(&hUsbDeviceFS))
  {
    return;
  }

  if (RingBuf_Used() == 0U)
  {
    return;
  }

  uint16_t count = RingBuf_Pop(gTxScratch, COMP_CDC_DATA_EP_SIZE);
  (void)USBD_COMPOSITE_CDC_Transmit(&hUsbDeviceFS, gTxScratch, count);
}

/*
 * Copy up to len bytes from buf into the ring buffer.
 * Bytes that exceed the available space are silently dropped.
 */
void CdcLog_Write(const char *buf, uint16_t len)
{
  uint16_t space = RingBuf_Free();
  uint16_t count = (len < space) ? len : space;

  for (uint16_t i = 0U; i < count; i++)
  {
    RingBuf_Push((uint8_t)buf[i]);
  }
}

/*
 * Format a string with printf-style arguments and write it to the ring buffer.
 * Uses a 128-byte stack scratch buffer; output longer than 128 bytes is truncated.
 */
void CdcLog_Printf(const char *fmt, ...)
{
  char    scratch[CDC_LOG_PRINTF_MAX];
  va_list args;

  va_start(args, fmt);
  int len = vsnprintf(scratch, sizeof(scratch), fmt, args);
  va_end(args);

  if (len > 0)
  {
    CdcLog_Write(scratch, (uint16_t)len);
  }
}
