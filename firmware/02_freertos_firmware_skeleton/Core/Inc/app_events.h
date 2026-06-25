/*
 * app_events.h
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

#ifndef INC_APP_EVENTS_H_
#define INC_APP_EVENTS_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Exported types ------------------------------------------------------------*/

/*
 * Application event types used by EventTask and WorkerTask.
 *
 * This project intentionally keeps the event model small:
 * - one type field
 * - one timestamp field
 * - one value field
 *
 * With three uint32_t fields, sizeof(AppEvent_t) should be 12 bytes.
 * This makes the queue capacity easy to explain:
 *
 *   8 queue slots * 12 bytes = 96 bytes payload capacity
 */
typedef enum
{
  APP_EVENT_SENSOR_SAMPLE     = 1,
  APP_EVENT_COMMAND_RECEIVED  = 2,
  APP_EVENT_ERROR_INJECT      = 3
} AppEventType_t;

typedef struct
{
  uint32_t type;
  uint32_t tick;
  uint32_t value;
} AppEvent_t;

/* Function prototypes -------------------------------------------------------*/
const char *AppEvent_GetName(uint32_t type);

#endif /* INC_APP_EVENTS_H_ */
