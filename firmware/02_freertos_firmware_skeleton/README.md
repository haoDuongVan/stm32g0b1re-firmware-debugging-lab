# 02 FreeRTOS Firmware Skeleton

FreeRTOS firmware skeleton project for NUCLEO-G0B1RE.

This project introduces a clean RTOS-based firmware structure using tasks, an event queue, a UART log mutex, measured counters, and a monitor task.

It is the second project in the STM32G0B1RE firmware debugging lab, after:

```txt
01_uart_dma_ring_logger
```

## Purpose

The goal of this project is to build a small but realistic FreeRTOS firmware skeleton.

The project focuses on:

```txt
Task separation
Periodic task timing
Queue-based event communication
Worker task processing
UART log serialization with a mutex
Queue full handling with dropped counter
Monitor-based runtime evidence
```

This project does not try to build an advanced logger.  
The advanced logger with logger task, ISR-safe API, log level, and production build macro will be handled in a later project.

## Target

| Item | Value |
|---|---|
| Board | NUCLEO-G0B1RE |
| MCU | STM32G0B1RE |
| IDE | STM32CubeIDE |
| System clock | 48 MHz |
| RTOS | FreeRTOS / CMSIS-RTOS |
| UART | USART2, 115200 bps |
| Debugger | On-board ST-LINK |
| Debug interface | SWD |
| LED | On-board user LED |

## Core Idea

The firmware runs fast periodic tasks, but logging is intentionally throttled.

```txt
Fast firmware timing:
- 10 ms task
- 100 ms event task

Human-readable debug output:
- 1000 ms monitor log
```

This avoids making UART output itself dominate the timing being measured.

## Task Design

### Fast Task

Period:

```txt
10 ms
```

Responsibility:

```txt
Simulate a fast periodic firmware task such as input scan, small polling, or state check.
```

Behavior:

```txt
fast_count++
```

The Fast Task does not print a log every 10 ms.  
The Monitor Task reports `fast_count` every 1000 ms.

### Event Generator Task

Period:

```txt
100 ms
```

Responsibility:

```txt
Create application events and send them to the Worker Task through a queue.
```

Behavior:

```txt
generated_count++
send AppEvent_t to App Event Queue
if queue is full, dropped_count++
```

### Worker Task

Period:

```txt
No fixed period
Waits on queue
```

Responsibility:

```txt
Receive events from the queue and process them.
```

Behavior:

```txt
wait for AppEvent_t
process event
processed_count++
```

The Worker Task simulates a processing time of about 20 ms per event.

### Heartbeat Task

Period:

```txt
500 ms
```

Responsibility:

```txt
Toggle the on-board LED and prove that the firmware is alive.
```

Behavior:

```txt
toggle LED
heartbeat_count++
```

The Heartbeat Task does not print a log every 500 ms.  
The Monitor Task reports `heartbeat_count`.

### Monitor Task

Period:

```txt
1000 ms
```

Responsibility:

```txt
Print measured system counters and queue status.
```

Example log:

```txt
[MONITOR] uptime=10000 fast=1000 heartbeat=20 generated=100 processed=100 dropped=0 queue=0
```

The Monitor Task must not create evidence by calculating counters from uptime.  
It only reads and prints measured counters that were incremented by the actual tasks.

## Counter Ownership

Each counter has one owner.

```txt
fast_count:
- incremented only by Fast Task every 10 ms

heartbeat_count:
- incremented only by Heartbeat Task every 500 ms

generated_count:
- incremented only by Event Generator Task when an event is created

processed_count:
- incremented only by Worker Task after an event is processed

dropped_count:
- incremented only when an event cannot be queued because the queue is full
```

Monitor Task:

```txt
does not calculate these counters from uptime
only reads and prints measured values
```

This keeps the evidence meaningful.  
For example, `heartbeat_count` must prove that Heartbeat Task really ran. It must not be computed as `uptime / 500`.

## Event Structure

The event structure is intentionally small and easy to reason about.

```c
typedef enum
{
    APP_EVENT_SENSOR_SAMPLE = 1,
    APP_EVENT_COMMAND_RECEIVED,
    APP_EVENT_ERROR_INJECT,
} AppEventType_t;

typedef struct
{
    uint32_t type;
    uint32_t tick;
    uint32_t value;
} AppEvent_t;
```

Expected size:

```txt
sizeof(AppEvent_t) = 12 bytes
```

Queue length:

```txt
8 slots
```

Queue payload capacity:

```txt
8 slots × 12 bytes = 96 bytes
```

This makes the queue stress test easy to explain mathematically.

## RTOS Objects

Required objects:

```txt
Tasks:
- Fast Task
- Event Generator Task
- Worker Task
- Heartbeat Task
- Monitor Task

Queue:
- App Event Queue
- length = 8
- item size = sizeof(AppEvent_t)

Mutex:
- UART Log Mutex
```

Optional later:

```txt
Binary semaphore for button/input interrupt
```

This project can start without a semaphore to keep the first RTOS skeleton simple.

## UART Log Mutex

The UART Log Mutex serializes log output from multiple tasks.

Without a mutex, two tasks may write UART logs at the same time and produce interleaved output such as:

```txt
[WOR[MONITOR]...
```

Expected behavior with the mutex:

```txt
[WORKER] processed id=10 value=123
[MONITOR] uptime=5000 fast=500 heartbeat=10 generated=50 processed=50 dropped=0 queue=0
```

Each log line should appear as a complete line.

## Test Modes

The project can be implemented with compile-time test modes or runtime flags.

Recommended test modes:

```txt
APP_TEST_MODE_NORMAL
APP_TEST_MODE_QUEUE_STRESS
```

Normal mode:

```txt
Fast Task: 10 ms
Event Generator Task: 100 ms
Worker processing: 20 ms/event
Heartbeat Task: 500 ms
Monitor Task: 1000 ms
```

Queue stress mode:

```txt
Every 1000 ms, generate a burst of 20 events.
Queue length is 8.
Some events should be accepted and some should be dropped.
```

## Test Scenarios

### Test 1 - Scheduler Timing

Purpose:

```txt
Verify that periodic tasks run at the expected timing.
```

Expected after about 10 seconds:

```txt
fast_count ≈ 1000
heartbeat_count ≈ 20
```

Example log:

```txt
[MONITOR] uptime=10000 fast=1000 heartbeat=20 generated=100 processed=100 dropped=0 queue=0
```

The values may have small deviation depending on startup timing and scheduling.

### Test 2 - Queue Communication and UART Mutex

Purpose:

```txt
Verify that Event Generator Task sends events and Worker Task receives them.
Verify that logs from Worker Task and Monitor Task are not interleaved.
```

Expected after about 10 seconds:

```txt
generated_count ≈ 100
processed_count ≈ 100
dropped_count = 0
queue = 0
```

Expected log quality:

```txt
No mixed lines such as:
[WOR[MONITOR]...
```

### Test 3 - Worker Processing Delay

Purpose:

```txt
Verify that the Worker Task can keep up with normal event rate.
```

Normal timing:

```txt
event period = 100 ms
worker processing time = 20 ms/event
```

Expected:

```txt
processed_count catches up with generated_count
queue does not continuously grow
dropped_count remains 0
```

### Test 4 - Queue Stress

Purpose:

```txt
Intentionally create a burst of events to verify queue full policy.
```

This is not normal behavior.  
It is an artificial burst used to simulate an abnormal input spike.

Stress condition:

```txt
Every 1000 ms:
- generate 20 events in a burst
- queue length = 8
- worker cannot consume all events immediately
```

Expected:

```txt
some events are accepted
some events are dropped
dropped_count increases
firmware does not crash
monitor task continues running
```

Example log:

```txt
[STRESS] burst=1 generated=20 accepted=8 dropped=12
[MONITOR] uptime=5000 fast=500 heartbeat=10 generated=100 processed=40 dropped=60 queue=8
```

The exact accepted/dropped count may vary depending on timing, but the queue full behavior must be visible and counted.

## Module Structure

Recommended structure:

```txt
Core/
├── Inc/
│   ├── app_main.h
│   ├── app_tasks.h
│   ├── app_events.h
│   └── uart_log.h
│
└── Src/
    ├── app_main.c
    ├── app_tasks.c
    ├── app_events.c
    └── uart_log.c
```

Suggested responsibilities:

```txt
app_main.c:
- app initialization
- RTOS object creation
- start scheduler

app_tasks.c:
- task entry functions
- counters
- monitor output

app_events.c:
- AppEvent_t definition helpers
- event name conversion
- event generator helper

uart_log.c:
- mutex-protected UART log function
```

## STM32CubeIDE Setup Notes

Recommended configuration:

```txt
SYS Debug: Serial Wire
System Clock: 48 MHz
USART2: Asynchronous, 115200 bps
GPIO: On-board user LED output
FreeRTOS: Enabled
CMSIS-RTOS: Enabled
```

Recommended FreeRTOS settings:

```txt
Use preemption: Enabled
Tick rate: 1000 Hz
Heap: choose a simple heap implementation suitable for the demo
```

The tick rate of 1000 Hz makes 10 ms and 100 ms task periods easy to express.

## Expected Normal Output

Example:

```txt
[BOOT] STM32G0B1RE Firmware Debugging Lab
[BOOT] Project: 02_freertos_firmware_skeleton
[BOOT] Board: NUCLEO-G0B1RE
[BOOT] System clock: 48 MHz
[BOOT] FreeRTOS kernel starting
[MONITOR] uptime=1000 fast=100 heartbeat=2 generated=10 processed=10 dropped=0 queue=0
[MONITOR] uptime=2000 fast=200 heartbeat=4 generated=20 processed=20 dropped=0 queue=0
[MONITOR] uptime=3000 fast=300 heartbeat=6 generated=30 processed=30 dropped=0 queue=0
```

## Evidence

Recommended evidence files:

```txt
assets/reports/02_freertos_firmware_skeleton.pdf
assets/reports/02_freertos_firmware_skeleton.txt

assets/screenshots/02_freertos_firmware_skeleton_build_success.png
assets/screenshots/02_freertos_firmware_skeleton_scheduler.png
assets/screenshots/02_freertos_firmware_skeleton_queue.png
assets/screenshots/02_freertos_firmware_skeleton_monitor.png
assets/screenshots/02_freertos_firmware_skeleton_queue_stress.png

assets/logs/02_freertos_firmware_skeleton_scheduler.txt
assets/logs/02_freertos_firmware_skeleton_queue.txt
assets/logs/02_freertos_firmware_skeleton_monitor.txt
assets/logs/02_freertos_firmware_skeleton_queue_stress.txt
assets/logs/02_freertos_firmware_skeleton_test_analysis.md
```

## Not Included

This project intentionally does not include:

```txt
Advanced logger task
Complete ISR-safe logger API
Production build macro
.map comparison
Flash EEPROM emulation
Bootloader
USB HID / CDC
```

These topics will be handled in later projects.

This project also intentionally does not include the following RTOS primitives and patterns, which require their own dedicated test setup to produce meaningful evidence:

```txt
Binary semaphore / counting semaphore for ISR signaling
Task Notification (ulTaskNotifyTake)
Event Group (xEventGroupWaitBits)
Race condition between task and ISR sharing one counter
Priority inversion / priority inheritance demonstration
HAL wrapper layer
```

The reason is structural, not just scope reduction. This skeleton gives every counter a single owner task (see Counter Ownership above), so there is no real task-vs-ISR race condition to observe here. Likewise, all tasks here run without a meaningful priority spread, so priority inversion cannot be demonstrated honestly with this firmware.

These RTOS primitives are planned for a dedicated follow-up project, `03_freertos_advanced_patterns`, which will need:

```txt
A real interrupt source (button or UART RX) firing faster than a task can consume it,
to compare binary semaphore signal loss against Task Notification's accumulated count.

A shared counter incremented from both a task and an ISR,
measured with and without a critical section, to show the lost-increment bug is real.

A scenario with at least two independent conditions a task must wait on together,
to justify Event Group over a simple queue or notification.

At least two projects sharing the same module structure but targeting different
UART instances or boards, to make the case for a HAL wrapper concrete.
```


## Related Projects

Previous:

```txt
01_uart_dma_ring_logger
```

Next planned:

```txt
03_flash_eeprom_emulation
```

Also planned, not yet scheduled:

```txt
03_freertos_advanced_patterns
(or merged into a later advanced-RTOS project: binary/counting semaphore,
Task Notification, Event Group, task-vs-ISR race condition, HAL wrapper)
```

## Final Scope

This project is a FreeRTOS firmware skeleton.

Core features:

```txt
Fast Task 10 ms
Event Generator Task 100 ms
Worker Task waits on queue
Heartbeat Task 500 ms
Monitor Task 1000 ms
App Event Queue
UART Log Mutex
Queue dropped counter
Measured counters owned by actual tasks
```
