/*
 * scan_scheduler.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_SCAN_SCHEDULER_H_
#define INC_SCAN_SCHEDULER_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Function prototypes -------------------------------------------------------*/

// Reset the request counter to zero; call once before starting the timer
void ScanScheduler_Init(void);

/*
 * Increment the pending scan request counter.
 * Call from the TIM6 period-elapsed ISR every 5 ms.
 * Counter is capped at SCAN_REQUEST_MAX_COUNT to prevent runaway growth
 * when the main loop is temporarily blocked.
 */
void ScanScheduler_OnTimerTick(void);

/*
 * Atomically decrement the counter and return 1 if a request was pending,
 * 0 if the counter was already zero.
 * Call in a while-loop so the main loop can drain all accumulated ticks
 * before moving on.
 */
uint8_t ScanScheduler_TakeRequest(void);

#endif /* INC_SCAN_SCHEDULER_H_ */
