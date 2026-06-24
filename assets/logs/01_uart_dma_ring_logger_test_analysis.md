# UART DMA Ring Buffer Logger - Test Analysis

Project:

```txt
firmware/01_uart_dma_ring_logger
```

Target:

```txt
Board: NUCLEO-G0B1RE
MCU: STM32G0B1RE
System clock: 48 MHz
UART: USART2, 115200 bps
DMA: USART2_TX
```

## 1. Normal Log Test

### Purpose

Verify that the UART DMA logger works correctly under normal low-load logging.

### Observed Result

The firmware prints one main log every 1 second:

```txt
[BOOT] Test mode: normal
[MAIN] tick=1000 dropped=0 buffered=0
[MAIN] tick=2000 dropped=0 buffered=0
[MAIN] tick=3000 dropped=0 buffered=0
...
[MAIN] tick=32000 dropped=0 buffered=0
```

### Analysis

The result shows that the UART DMA logger works normally when the log rate is low.

The important points are:

```txt
dropped=0
buffered=0
```

This means:

- every log message was accepted by the ring buffer
- UART DMA was able to transmit the queued data fast enough
- the ring buffer returned to empty after each transmission
- no log message was dropped

### Conclusion

```txt
[OK] Normal periodic logging works
[OK] No dropped messages
[OK] Ring buffer does not accumulate data under low load
```

---

## 2. Blocking UART Transmit Test

### Purpose

Compare the UART DMA logger with a blocking UART transmit loop using `HAL_UART_Transmit()`.

### Observed Result

The blocking test starts around tick 2501:

```txt
[TEST] blocking printf test start tick=2501
```

The firmware then prints 500 blocking messages from index 0 to index 499.

The test finishes around tick 5752:

```txt
[TEST] blocking printf test done tick=5752
```

After the blocking loop finishes, the main log resumes:

```txt
[MAIN] tick=5756 dropped=0 buffered=0
[MAIN] tick=6756 dropped=0 buffered=0
[MAIN] tick=7756 dropped=0 buffered=0
```

### Timing Analysis

The blocking section takes approximately:

```txt
5752 ms - 2501 ms = 3251 ms
```

During this period, the CPU is busy sending UART data with blocking transmit.

Because the CPU is blocked inside `HAL_UART_Transmit()`, the regular main loop processing is delayed.

This is visible because the normal `[MAIN]` log does not appear at tick 3000, 4000, or 5000.  
Instead, the next main log appears only after the blocking loop finishes:

```txt
[MAIN] tick=5756 dropped=0 buffered=0
```

### Conclusion

```txt
[OK] Blocking UART transmit delays the main loop
[OK] The delay is visible in the tick log
[OK] This test demonstrates why blocking UART logging is dangerous for firmware timing
```

---

## 3. Spam Log Test

### Purpose

Verify that many log messages can be queued through the UART DMA ring buffer logger without blocking the main firmware flow.

### Observed Result

The spam test starts at tick 2000:

```txt
[TEST] spam log start
```

The visible spam messages are printed from index 0 to index 75:

```txt
[SPAM] index=0 tick=2000
...
[SPAM] index=75 tick=2005
```

After the spam test, the main loop continues running:

```txt
[MAIN] tick=3000 dropped=26 buffered=0
[MAIN] tick=4000 dropped=26 buffered=0
[MAIN] tick=5000 dropped=26 buffered=0
```

### Drop Count Analysis

The spam loop runs 100 times:

```txt
index 0-99
```

Observed behavior:

```txt
index 0-75:   76 messages accepted
index 76-99:  24 messages dropped
```

The final test summary message is also dropped because the ring buffer is already full:

```txt
[TEST] spam log queued...
```

The third dropped message is `[MAIN] tick=2000`, not `[MAIN] tick=3000`.

`App_RunSpamTest()` and the periodic tick check both run inside the same `App_Run()` call at tick=2000. After the spam test fills the buffer, `now` still holds the stale value `2000`. The periodic check `(now - last_tick) >= ONE_SECOND` evaluates to true, so `[MAIN] tick=2000` is attempted immediately — but the buffer is still full, so it is dropped.

`[MAIN] tick=3000` is visible in the log with `buffered=0`, confirming the ring buffer had already drained by then.

Therefore:

```txt
24 dropped spam messages  (index 76-99)
+ 1 dropped test summary  ([TEST] spam log queued...)
+ 1 dropped [MAIN] tick=2000  (same App_Run call, buffer still full)
= 26 dropped messages
```

This matches the observed result:

```txt
[MAIN] tick=3000 dropped=26 buffered=0
```

### UART Speed Analysis

UART is configured as:

```txt
115200 bps, 8N1
```

With 8N1 format, one data byte takes about 10 bits on the wire:

```txt
115200 bit/s / 10 = 11520 byte/s
≈ 11.5 byte/ms
```

The CPU can enqueue log messages much faster than UART can transmit them.

Approximate behavior:

```txt
UART transmit speed ≈ 11.5 byte/ms
CPU enqueue speed   >> UART transmit speed
```

Therefore, during a burst:

```txt
ring buffer fill rate = enqueue rate - UART transmit rate
```

Once the ring buffer no longer has enough free space for a complete message, the logger drops the whole message and increments `dropped_count`.

### Conclusion

```txt
[OK] Spam logging does not block the firmware
[OK] UART DMA continues transmitting queued logs
[OK] Ring buffer absorbs the burst temporarily
[OK] When the buffer is full, complete messages are dropped
[OK] dropped_count correctly reports dropped messages
```

---

## 4. Buffer Full Test

### Purpose

Verify logger behavior when the ring buffer becomes full under a heavier burst.

### Observed Result

The buffer full test starts at tick 2000:

```txt
[TEST] buffer full test start
```

Visible messages are printed from index 0 to index 27:

```txt
[FILL] index=0 tick=2000 payload=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
...
[FILL] index=27 tick=2002 payload=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
```

After the burst, the main loop continues:

```txt
[MAIN] tick=3000 dropped=474 buffered=0
[MAIN] tick=4000 dropped=474 buffered=0
[MAIN] tick=5000 dropped=474 buffered=0
```

### Drop Count Analysis

The buffer full test sends a much larger burst than the spam test.

Most messages cannot fit into the ring buffer once it becomes full.

The observed result is:

```txt
dropped=474
```

This means the logger did not block the CPU and did not overwrite old data.  
Instead, it rejected complete messages when there was not enough free buffer space.

### Conclusion

```txt
[OK] Ring buffer full condition is handled safely
[OK] Logger does not block the main firmware flow
[OK] Logger does not crash
[OK] Logger does not silently overwrite old data
[OK] dropped_count makes message loss visible
```

---

## 5. Overall Result

This project demonstrates the difference between blocking UART logging and UART DMA logging with a ring buffer.

### Blocking UART transmit

```txt
Pros:
- simple implementation
- easy to understand

Cons:
- CPU waits for UART transmission
- main loop timing is delayed
- bad for frequent logs or timing-sensitive firmware
```

### UART DMA ring buffer logger

```txt
Pros:
- application can enqueue logs quickly
- UART transfer runs in the background using DMA
- main loop continues running
- burst logs can be absorbed by the ring buffer
- message loss is visible through dropped_count

Cons:
- implementation is more complex
- buffer size must be designed carefully
- logs can still be dropped if the producer is faster than UART for too long
```

## Final Conclusion

The UART DMA ring buffer logger is better suited for firmware debug logging than blocking UART transmit.

However, DMA does not remove the bandwidth limit of UART.

The ring buffer only absorbs temporary bursts.  
If log production continues faster than UART transmission, the buffer eventually becomes full.

This project handles that condition explicitly by dropping complete messages and incrementing `dropped_count`.
