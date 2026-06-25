/*
 * app_tasks.c
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "app_tasks.h"
#include "app_events.h"
#include "main.h"
#include "uart_log.h"

/* Private defines -----------------------------------------------------------*/

#define FAST_TASK_PERIOD                 10U     // ms
#define EVENT_TASK_PERIOD                100U    // ms
#define HEARTBEAT_TASK_PERIOD            500U    // ms
#define MONITOR_TASK_PERIOD              1000U   // ms
#define WORKER_PROCESS_TIME              20U     // ms

#define STRESS_BURST_PERIOD              1000U   // ms
#define STRESS_BURST_EVENT_COUNT         20U

/*
 * The actual CubeIDE-generated GPIO names in this project are:
 *
 *   LED_GREEN_Pin
 *   LED_GREEN_GPIO_Port
 *
 * They are generated in Core/Inc/main.h.
 * Do not use LD2_Pin or gpio.h in this project.
 */
#define APP_LED_GPIO_Port                LED_GREEN_GPIO_Port
#define APP_LED_Pin                      LED_GREEN_Pin

/* Private variables ---------------------------------------------------------*/

static osMessageQueueId_t app_event_queue = NULL;

/*
 * Counter ownership rule:
 *
 * fast_count       : incremented only by FastTask
 * heartbeat_count  : incremented only by HeartbeatTask
 * generated_count  : incremented only by EventTask when an event is created
 * processed_count  : incremented only by WorkerTask after processing an event
 * dropped_count    : incremented only when queue send fails
 *
 * MonitorTask only reads these values. It does not calculate them from uptime.
 */
static volatile uint32_t fast_count = 0U;
static volatile uint32_t heartbeat_count = 0U;
static volatile uint32_t generated_count = 0U;
static volatile uint32_t processed_count = 0U;
static volatile uint32_t dropped_count = 0U;

static osThreadId_t fast_task_handle = NULL;
static osThreadId_t event_task_handle = NULL;
static osThreadId_t worker_task_handle = NULL;
static osThreadId_t heartbeat_task_handle = NULL;
static osThreadId_t monitor_task_handle = NULL;

/*
 * CMSIS-RTOS v2 uses byte units for stack_size.
 * Example: 256 * 4 means 256 words = 1024 bytes.
 */
static const osThreadAttr_t fast_task_attributes = {
  .name       = "FastTask",
  .priority   = (osPriority_t)osPriorityAboveNormal,
  .stack_size = 192 * 4
};

static const osThreadAttr_t event_task_attributes = {
  .name       = "EventTask",
  .priority   = (osPriority_t)osPriorityNormal,
  .stack_size = 256 * 4
};

static const osThreadAttr_t worker_task_attributes = {
  .name       = "WorkerTask",
  .priority   = (osPriority_t)osPriorityNormal,
  .stack_size = 256 * 4
};

static const osThreadAttr_t heartbeat_task_attributes = {
  .name       = "HeartbeatTask",
  .priority   = (osPriority_t)osPriorityLow,
  .stack_size = 192 * 4
};

static const osThreadAttr_t monitor_task_attributes = {
  .name       = "MonitorTask",
  .priority   = (osPriority_t)osPriorityNormal,
  .stack_size = 384 * 4
};

/* Private function prototypes -----------------------------------------------*/

static void StartFastTask(void *argument);
static void StartEventTask(void *argument);
static void StartWorkerTask(void *argument);
static void StartHeartbeatTask(void *argument);
static void StartMonitorTask(void *argument);

static void AppTasks_SendEvent(uint32_t type, uint32_t value);
static uint32_t AppTasks_SelectEventType(uint32_t sequence);

/* Function definitions ------------------------------------------------------*/

void AppTasks_Init(osMessageQueueId_t event_queue)
{
  app_event_queue = event_queue;
}

void AppTasks_Create(void)
{
  fast_task_handle = osThreadNew(StartFastTask, NULL, &fast_task_attributes);
  event_task_handle = osThreadNew(StartEventTask, NULL, &event_task_attributes);
  worker_task_handle = osThreadNew(StartWorkerTask, NULL, &worker_task_attributes);
  heartbeat_task_handle = osThreadNew(StartHeartbeatTask, NULL, &heartbeat_task_attributes);
  monitor_task_handle = osThreadNew(StartMonitorTask, NULL, &monitor_task_attributes);

  /*
   * Print task creation result before osKernelStart().
   * If configTOTAL_HEAP_SIZE is too small, one or more handles will be NULL.
   */
  UartLog_Printf("[BOOT] FastTask=%s EventTask=%s WorkerTask=%s HeartbeatTask=%s MonitorTask=%s\r\n",
                 (fast_task_handle != NULL) ? "OK" : "NG",
                 (event_task_handle != NULL) ? "OK" : "NG",
                 (worker_task_handle != NULL) ? "OK" : "NG",
                 (heartbeat_task_handle != NULL) ? "OK" : "NG",
                 (monitor_task_handle != NULL) ? "OK" : "NG");
}

void AppTasks_GetCounterSnapshot(AppTaskCounterSnapshot_t *snapshot)
{
  if (snapshot == NULL) {
    return;
  }

  snapshot->fast_count = fast_count;
  snapshot->heartbeat_count = heartbeat_count;
  snapshot->generated_count = generated_count;
  snapshot->processed_count = processed_count;
  snapshot->dropped_count = dropped_count;

  if (app_event_queue != NULL) {
    snapshot->queue_count = osMessageQueueGetCount(app_event_queue);
  } else {
    snapshot->queue_count = 0U;
  }
}

/* Private functions ---------------------------------------------------------*/

/*
 * Create one AppEvent_t and send it to the App Event Queue.
 *
 * Queue full policy:
 * - The queue has only APP_EVENT_QUEUE_LENGTH slots.
 * - If the queue is full, the new event is dropped.
 * - dropped_count is incremented so the overflow is visible in MonitorTask.
 *
 * This is intentional. Queue overflow must be measurable, not hidden.
 */
static void AppTasks_SendEvent(uint32_t type, uint32_t value)
{
  AppEvent_t event;
  osStatus_t status;

  if (app_event_queue == NULL) {
    dropped_count++;
    return;
  }

  event.type = type;
  event.tick = osKernelGetTickCount();
  event.value = value;

  generated_count++;

  status = osMessageQueuePut(app_event_queue, &event, 0U, 0U);

  if (status != osOK) {
    dropped_count++;
  }
}

static uint32_t AppTasks_SelectEventType(uint32_t sequence)
{
  switch (sequence % 3U)
  {
    case 0U:
      return APP_EVENT_SENSOR_SAMPLE;

    case 1U:
      return APP_EVENT_COMMAND_RECEIVED;

    default:
      return APP_EVENT_ERROR_INJECT;
  }
}

/*
 * FastTask simulates a fast 10 ms firmware task.
 *
 * It must not print UART logs every cycle because UART output would dominate
 * the timing being measured. The task only increments fast_count.
 * MonitorTask reports fast_count once per second.
 */
static void StartFastTask(void *argument)
{
  uint32_t next_tick;

  (void)argument;

  next_tick = osKernelGetTickCount();

  for (;;)
  {
    fast_count++;

    next_tick += FAST_TASK_PERIOD;
    (void)osDelayUntil(next_tick);
  }
}

/*
 * EventTask creates application events.
 *
 * Normal mode:
 * - 1 event every 100 ms
 * - WorkerTask processing time is 20 ms
 * - WorkerTask should keep up
 *
 * Queue stress mode:
 * - 20 events are generated in one burst every 1000 ms
 * - Queue length is only 8
 * - Some events should be dropped and counted
 */
static void StartEventTask(void *argument)
{
  uint32_t sequence = 0U;
  uint32_t next_tick;

  (void)argument;

  next_tick = osKernelGetTickCount();

#if APP_TEST_MODE == APP_TEST_MODE_QUEUE_STRESS
  uint32_t burst_id = 0U;

  for (;;)
  {
    uint32_t before_generated;
    uint32_t before_dropped;
    uint32_t generated_delta;
    uint32_t dropped_delta;
    uint32_t accepted_delta;

    next_tick += STRESS_BURST_PERIOD;
    (void)osDelayUntil(next_tick);

    before_generated = generated_count;
    before_dropped = dropped_count;

    for (uint32_t i = 0U; i < STRESS_BURST_EVENT_COUNT; i++)
    {
      AppTasks_SendEvent(AppTasks_SelectEventType(sequence), sequence);
      sequence++;
    }

    generated_delta = generated_count - before_generated;
    dropped_delta = dropped_count - before_dropped;
    accepted_delta = generated_delta - dropped_delta;

    burst_id++;

    UartLog_Printf("[STRESS] burst=%lu generated=%lu accepted=%lu dropped=%lu\r\n",
                   (unsigned long)burst_id,
                   (unsigned long)generated_delta,
                   (unsigned long)accepted_delta,
                   (unsigned long)dropped_delta);
  }
#else
  for (;;)
  {
    next_tick += EVENT_TASK_PERIOD;
    (void)osDelayUntil(next_tick);

    AppTasks_SendEvent(AppTasks_SelectEventType(sequence), sequence);
    sequence++;
  }
#endif
}

/*
 * WorkerTask waits for events and processes them.
 *
 * It has no fixed period. It blocks on the queue until an event arrives.
 * The 20 ms delay simulates real processing work.
 */
static void StartWorkerTask(void *argument)
{
  AppEvent_t event;
  osStatus_t status;

  (void)argument;

  for (;;)
  {
    if (app_event_queue == NULL) {
      (void)osDelay(100U);
      continue;
    }

    status = osMessageQueueGet(app_event_queue, &event, NULL, osWaitForever);

    if (status == osOK) {
      (void)osDelay(WORKER_PROCESS_TIME);

      processed_count++;

      /*
       * Print occasionally to verify UART log mutex.
       * MonitorTask also prints once per second.
       * Each log line should stay complete and should not be interleaved.
       */
      if ((processed_count % 10U) == 0U) {
        UartLog_Printf("[WORKER] processed=%lu event=%s value=%lu tick=%lu\r\n",
                       (unsigned long)processed_count,
                       AppEvent_GetName(event.type),
                       (unsigned long)event.value,
                       (unsigned long)event.tick);
      }
    }
  }
}

/*
 * HeartbeatTask toggles the on-board LED every 500 ms.
 *
 * heartbeat_count is incremented by this task only.
 * MonitorTask reads heartbeat_count and prints it once per second.
 */
static void StartHeartbeatTask(void *argument)
{
  uint32_t next_tick;

  (void)argument;

  next_tick = osKernelGetTickCount();

  for (;;)
  {
    HAL_GPIO_TogglePin(APP_LED_GPIO_Port, APP_LED_Pin);
    heartbeat_count++;

    next_tick += HEARTBEAT_TASK_PERIOD;
    (void)osDelayUntil(next_tick);
  }
}

/*
 * MonitorTask prints the measured system state once per second.
 *
 * It does not calculate fast_count or heartbeat_count from uptime.
 * It only reads counters updated by their owner tasks.
 */
static void StartMonitorTask(void *argument)
{
  uint32_t next_tick;

  (void)argument;

  next_tick = osKernelGetTickCount();

  for (;;)
  {
    AppTaskCounterSnapshot_t snapshot;

    next_tick += MONITOR_TASK_PERIOD;
    (void)osDelayUntil(next_tick);

    AppTasks_GetCounterSnapshot(&snapshot);

    UartLog_Printf("[MONITOR] uptime=%lu fast=%lu heartbeat=%lu generated=%lu processed=%lu dropped=%lu queue=%lu\r\n",
                   (unsigned long)HAL_GetTick(),
                   (unsigned long)snapshot.fast_count,
                   (unsigned long)snapshot.heartbeat_count,
                   (unsigned long)snapshot.generated_count,
                   (unsigned long)snapshot.processed_count,
                   (unsigned long)snapshot.dropped_count,
                   (unsigned long)snapshot.queue_count);
  }
}
