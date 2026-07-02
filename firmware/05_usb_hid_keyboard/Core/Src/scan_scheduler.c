/*
 * scan_scheduler.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "scan_scheduler.h"
#include "stm32g0xx_hal.h"

/* Private defines -----------------------------------------------------------*/

/*
 * Maximum number of unserviced scan requests that can accumulate.
 * If the main loop is blocked for longer than 10 × 5 ms = 50 ms,
 * further timer ticks are silently dropped rather than letting the
 * counter grow without bound.
 */
#define SCAN_REQUEST_MAX_COUNT  10U

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t gScanRequestCount = 0U;

/* Function definitions ------------------------------------------------------*/

// Reset the request counter to zero; call once before starting the timer
void ScanScheduler_Init(void)
{
  gScanRequestCount = 0U;
}

// Increment the pending scan request counter; called from TIM6 ISR every 5 ms
void ScanScheduler_OnTimerTick(void)
{
  if (gScanRequestCount < SCAN_REQUEST_MAX_COUNT)
  {
    gScanRequestCount++;
  }
}

// Atomically consume one request; returns 1 if a request was pending, else 0
uint8_t ScanScheduler_TakeRequest(void)
{
  uint8_t hasRequest = 0U;

  __disable_irq();

  if (gScanRequestCount > 0U)
  {
    gScanRequestCount--;
    hasRequest = 1U;
  }

  __enable_irq();

  return hasRequest;
}
