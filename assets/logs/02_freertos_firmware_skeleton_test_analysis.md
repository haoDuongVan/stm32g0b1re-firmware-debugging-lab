# 02 FreeRTOS Firmware Skeleton - Test Analysis

This file summarizes the runtime evidence for `02_freertos_firmware_skeleton`.

The project verifies a small FreeRTOS firmware skeleton on `NUCLEO-G0B1RE` using measured counters, an application event queue, a UART log mutex, and a monitor task.

## 1. Test Environment

| Item | Value |
|---|---|
| Board | NUCLEO-G0B1RE |
| MCU | STM32G0B1RE |
| System clock | 48 MHz |
| RTOS | FreeRTOS / CMSIS-RTOS v2 |
| UART | USART2, 115200 bps |
| Debug output | UART log |
| App event queue length | 8 events |
| Event size | 12 bytes |
| Queue payload capacity | 8 × 12 bytes = 96 bytes |

The application uses the following tasks:

```txt
FastTask        : 10 ms
EventTask       : 100 ms in normal mode
WorkerTask      : waits on AppEventQueue
HeartbeatTask   : 500 ms
MonitorTask     : 1000 ms
```

Counter ownership rule:

```txt
fast_count       -> incremented only by FastTask
heartbeat_count  -> incremented only by HeartbeatTask
generated_count  -> incremented only by EventTask
processed_count  -> incremented only by WorkerTask
dropped_count    -> incremented only when queue send fails
```

`MonitorTask` only reads and prints these measured counters. It does not calculate them from uptime.

---

## 2. Normal Mode Test

### 2.1 Purpose

Normal mode verifies the basic RTOS skeleton behavior:

```txt
FastTask periodic timing
HeartbeatTask periodic timing
EventTask to WorkerTask queue communication
WorkerTask processing under normal load
UART log serialization by mutex
```

In normal mode, `EventTask` creates one event every 100 ms. `WorkerTask` simulates about 20 ms of processing time per event. Since the worker processing time is much shorter than the event period, the queue should not accumulate pending events.

### 2.2 Observed Result

At around 59 seconds after boot, the log shows:

```txt
[MONITOR] uptime=59001 fast=5901 heartbeat=118 generated=589 processed=589 dropped=0 queue=0
```

### 2.3 Scheduler Timing Analysis

For `FastTask`:

```txt
uptime ≈ 59001 ms
FastTask period = 10 ms
expected fast_count ≈ 5900
observed fast_count = 5901
```

The observed value is very close to the expected value. This confirms that `FastTask` is running periodically at approximately 10 ms.

For `HeartbeatTask`:

```txt
uptime ≈ 59001 ms
HeartbeatTask period = 500 ms
expected heartbeat_count ≈ 118
observed heartbeat_count = 118
```

This confirms that `HeartbeatTask` is running periodically at approximately 500 ms.

### 2.4 Queue Communication Analysis

At around 59 seconds:

```txt
generated = 589
processed = 589
dropped   = 0
queue     = 0
```

This shows that `WorkerTask` can keep up with the normal event generation rate.

The queue does not accumulate pending events, and no event is dropped.

### 2.5 UART Mutex Check

Both `WorkerTask` and `MonitorTask` print UART logs.

The captured log does not show interleaved or broken lines such as:

```txt
[WOR[MONITOR]...
```

Each UART log appears as a complete line. This confirms that the UART log mutex is working as intended in this test.

### 2.6 Normal Mode Conclusion

Normal mode passed.

The firmware successfully demonstrated:

```txt
10 ms periodic task execution
500 ms heartbeat task execution
100 ms event generation
queue-based event communication
worker processing without backlog
dropped_count remaining at 0
UART log serialization by mutex
```

---

## 3. Queue Stress Mode Test

### 3.1 Purpose

Queue stress mode intentionally creates an abnormal event burst to verify the queue full policy.

This test is not meant to represent normal application behavior. It is designed to confirm that the firmware handles queue overflow in a visible and controlled way.

Stress condition:

```txt
Every 1000 ms:
- generate 20 events in a burst
- queue length = 8 events
```

Expected behavior:

```txt
8 events are accepted into the queue
12 events are dropped
dropped_count increases
firmware continues running
MonitorTask continues printing system state
```

### 3.2 Observed Burst Behavior

The log repeatedly shows the following pattern:

```txt
[STRESS] burst=1 generated=20 accepted=8 dropped=12
[STRESS] burst=2 generated=20 accepted=8 dropped=12
[STRESS] burst=3 generated=20 accepted=8 dropped=12
```

This directly matches the queue design:

```txt
queue length = 8
burst size   = 20
accepted     = 8
dropped      = 20 - 8 = 12
```

### 3.3 Long Run Result

The last complete monitor line used for analysis is:

```txt
[MONITOR] uptime=211002 fast=21101 heartbeat=422 generated=4220 processed=1680 dropped=2532 queue=8
```

The file continues with `burst=212`, but the final monitor line after that burst is not complete in the captured text. Therefore, the analysis uses the last complete monitor snapshot at `uptime=211002`.

### 3.4 Scheduler Timing During Stress

Even under queue stress, the periodic counters continue to match their expected timing.

For `FastTask`:

```txt
uptime ≈ 211002 ms
FastTask period = 10 ms
expected fast_count ≈ 21100
observed fast_count = 21101
```

For `HeartbeatTask`:

```txt
uptime ≈ 211002 ms
HeartbeatTask period = 500 ms
expected heartbeat_count ≈ 422
observed heartbeat_count = 422
```

This confirms that queue overflow does not stop the scheduler or the periodic tasks.

### 3.5 Queue Full Accounting

At the last complete monitor snapshot:

```txt
generated = 4220
processed = 1680
dropped   = 2532
queue     = 8
```

The accounting is consistent:

```txt
processed + dropped + queue = 1680 + 2532 + 8 = 4220
generated = 4220
```

This is an important result. It shows that the firmware does not lose events silently.

Every generated event is accounted for as one of the following:

```txt
processed
dropped
still pending in queue
```

### 3.6 Per-Burst Accounting

At `uptime=211002`, the total generated count is:

```txt
generated = 4220
```

Since each stress burst generates 20 events:

```txt
4220 / 20 = 211 bursts
```

For 211 complete bursts, the expected accepted and dropped counts are:

```txt
accepted = 211 × 8  = 1688
dropped  = 211 × 12 = 2532
```

The observed dropped count is:

```txt
dropped = 2532
```

The observed accepted count is represented by processed events plus events still pending in the queue:

```txt
accepted = processed + queue
accepted = 1680 + 8 = 1688
```

This matches the expected result exactly.

### 3.7 WorkerTask Behavior During Stress

`WorkerTask` continues processing accepted events during the stress test.

Example worker logs:

```txt
[WORKER] processed=10 event=SENSOR_SAMPLE value=21 tick=2000
[WORKER] processed=100 event=SENSOR_SAMPLE value=243 tick=13000
[WORKER] processed=1000 event=SENSOR_SAMPLE value=2487 tick=125000
```

This confirms that the worker does not stop when the queue becomes full. It continues to process the accepted events while newly overflowing events are counted as dropped.

### 3.8 Queue Stress Conclusion

Queue stress mode passed.

The firmware successfully demonstrated:

```txt
queue full detection
controlled event drop policy
dropped_count increment on queue full
continued WorkerTask processing
continued MonitorTask output
continued 10 ms and 500 ms periodic task execution
no crash or scheduler stop under repeated queue overflow
```

---

## 4. Overall Conclusion

Both normal mode and queue stress mode passed.

Normal mode confirms that the FreeRTOS skeleton works correctly under expected load:

```txt
FastTask runs every 10 ms
HeartbeatTask runs every 500 ms
EventTask generates events every 100 ms
WorkerTask processes all generated events
dropped_count stays at 0
queue stays empty
```

Queue stress mode confirms that the event queue overflow policy works correctly:

```txt
20 events are generated per burst
8 events are accepted
12 events are dropped
dropped_count tracks dropped events
processed + dropped + queued = generated
```

The most important result is that the firmware makes overload visible.

Instead of silently losing events, the system counts dropped events and keeps running. This follows the same design principle as the previous UART DMA Ring Buffer Logger project: bounded buffers must have an explicit overflow policy and measurable evidence.
