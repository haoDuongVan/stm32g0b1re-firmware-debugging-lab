/*
 * app_events.c
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "app_events.h"

/* Function definitions ------------------------------------------------------*/

/*
 * Convert an event type to a short text name for UART logs.
 *
 * This keeps WorkerTask logs readable without placing switch-case text
 * directly inside the task implementation.
 */
const char *AppEvent_GetName(uint32_t type)
{
  switch ((AppEventType_t)type)
  {
    case APP_EVENT_SENSOR_SAMPLE:
      return "SENSOR_SAMPLE";

    case APP_EVENT_COMMAND_RECEIVED:
      return "COMMAND_RECEIVED";

    case APP_EVENT_ERROR_INJECT:
      return "ERROR_INJECT";

    default:
      return "UNKNOWN";
  }
}
