/*
 * app_tasks.h
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

#ifndef INC_APP_TASKS_H_
#define INC_APP_TASKS_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include "cmsis_os.h"

/* Exported defines ----------------------------------------------------------*/

#define APP_EVENT_QUEUE_LENGTH           8U

#define APP_TEST_MODE_NORMAL             0U
#define APP_TEST_MODE_QUEUE_STRESS       1U

/*
 * Default test mode.
 *
 * Change this to APP_TEST_MODE_QUEUE_STRESS when verifying queue full behavior.
 */
#define APP_TEST_MODE                    APP_TEST_MODE_NORMAL

/* Exported types ------------------------------------------------------------*/

/*
 * MonitorTask prints this snapshot once per second.
 *
 * Important:
 * These counters are measured values. MonitorTask must not calculate them
 * from uptime. Each counter is incremented only by its owner task.
 */
typedef struct
{
  uint32_t fast_count;
  uint32_t heartbeat_count;
  uint32_t generated_count;
  uint32_t processed_count;
  uint32_t dropped_count;
  uint32_t queue_count;
} AppTaskCounterSnapshot_t;

/* Function prototypes -------------------------------------------------------*/
void AppTasks_Init(osMessageQueueId_t event_queue);
void AppTasks_Create(void);
void AppTasks_GetCounterSnapshot(AppTaskCounterSnapshot_t *snapshot);

#endif /* INC_APP_TASKS_H_ */
