# 01 UART DMA Ring Buffer Logger

Bare-metal UART DMA logger project for NUCLEO-G0B1RE.

This project compares simple blocking UART output with a non-blocking UART TX DMA logger backed by a ring buffer.

## Purpose

The goal of this project is to demonstrate a practical UART debug logger that can queue log messages without blocking the main firmware flow.

The project focuses on two related problems:

```txt
HAL_UART_Transmit()
  -> blocks the CPU while UART is transmitting

HAL_UART_Transmit_DMA()
  -> starts one DMA transfer, but does not queue messages automatically
```

The solution used in this project is:

```txt
Application
  -> DebugLogger_Printf()
  -> temporary formatting buffer
  -> ring buffer
  -> USART2 TX DMA
  -> DMA TX complete callback
  -> start next DMA transfer if queued data remains
```

This is the first main debugging project after `00_base_bringup`.

## Target

| Item | Value |
|---|---|
| Board | NUCLEO-G0B1RE |
| MCU | STM32G0B1RE |
| IDE | STM32CubeIDE |
| System clock | 48 MHz |
| Debugger | On-board ST-LINK |
| Debug interface | SWD |
| UART | USART2, 115200 bps |
| DMA | USART2_TX |
| LED | On-board user LED |

## Problem

Blocking UART transmit is simple, but it can disturb firmware timing.

Example:

```c
HAL_UART_Transmit(&huart2, data, len, HAL_MAX_DELAY);
```

This waits until the UART transmission finishes.

At 115200 bps, UART is much slower than the CPU.  
With 8N1 UART format, one byte normally takes about 10 bits on the wire, so 115200 bps is roughly 11.5 KB/s.

If the firmware prints many logs with blocking transmit, the main loop can be delayed.

Changing only to DMA is not enough:

```c
HAL_UART_Transmit_DMA(&huart2, data, len);
```

DMA does not automatically queue messages.  
If the UART DMA transfer is still active, the next call may return `HAL_BUSY`.

This project adds a ring buffer between the application and UART DMA.

## Architecture

```txt
Application
  |
  v
DebugLogger_Printf()
  |
  v
Temporary format buffer
  |
  v
Ring Buffer
  |
  v
HAL_UART_Transmit_DMA()
  |
  v
DMA TX Complete Callback
  |
  v
Advance ring buffer tail by transmitted length
  |
  v
Start next DMA transfer if more data exists
```

Important implementation rule:

```txt
The DMA complete callback must advance the ring buffer tail only by the transmitted length.

It must not do:
tail = head
```

New log data may be written into the ring buffer while DMA is transmitting the current chunk.  
Setting `tail = head` in the callback would accidentally discard data that was queued during the active transfer.

## Main Modules

```txt
Core/
├── Inc/
│   ├── app_main.h
│   ├── ring_buffer.h
│   └── uart_logger.h
│
└── Src/
    ├── app_main.c
    ├── ring_buffer.c
    └── uart_logger.c
```

## Implementation Scope

Included in this version:

- bare-metal main loop
- USART2 TX DMA
- ring buffer
- `DebugLogger_Printf()`
- DMA TX complete callback handling
- dropped message counter
- normal log test
- blocking UART transmit comparison test
- spam log test
- buffer full test

Not included in this version:

- FreeRTOS mutex logger
- ISR-safe logger API
- dedicated logger task
- production build macro
- `.map` comparison
- dead code elimination check
- logic analyzer benchmark

Those topics will be handled in later projects.

## STM32CubeIDE Setup Notes

Recommended `.ioc` configuration:

```txt
SYS Debug: Serial Wire
System Clock: 48 MHz
USART2 Mode: Asynchronous
USART2 Baudrate: 115200
USART2 TX DMA: Enabled
GPIO: On-board user LED as output
```

Recommended DMA setting for USART2 TX:

```txt
DMA request: USART2_TX
Mode: Normal
Data width: Byte
Memory increment: Enabled
Peripheral increment: Disabled
```

DMA interrupt should be enabled so that the logger can start the next transfer from the TX complete callback.

## Test Mode Selection

The demo application supports four compile-time test modes in `app_main.c`:

```c
#define APP_TEST_MODE_NORMAL            0
#define APP_TEST_MODE_SPAM              1
#define APP_TEST_MODE_BUFFER_FULL       2
#define APP_TEST_MODE_BLOCKING_PRINTF   3
```

The default mode should be:

```c
#define APP_TEST_MODE APP_TEST_MODE_NORMAL
```

Use the other modes when collecting evidence for the specific test scenario.

## Expected Behavior

After flashing the firmware in normal mode:

1. The on-board LED toggles periodically.
2. The UART terminal prints boot logs through the DMA logger.
3. The UART terminal prints tick logs.
4. The dropped counter remains 0 in normal low-load logging.

Example normal output:

```txt
[BOOT] STM32G0B1RE Firmware Debugging Lab
[BOOT] Project: 01_uart_dma_ring_logger
[BOOT] Board: NUCLEO-G0B1RE
[BOOT] System clock: 48 MHz
[BOOT] UART DMA logger initialized
[BOOT] Test mode: normal
[MAIN] tick=1000 dropped=0 buffered=0
[MAIN] tick=2000 dropped=0 buffered=0
[MAIN] tick=3000 dropped=0 buffered=0
```

## UART Terminal Settings

```txt
Baudrate: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
Line ending: CRLF
```

## Test Scenarios

### Test 1 - Normal Log

Purpose:

```txt
Verify that UART DMA logger works in a normal low-load condition.
```

Expected result:

```txt
[OK] LED toggles periodically
[OK] UART log is printed through DMA logger
[OK] dropped counter remains 0
[OK] buffered size returns to 0 after transmission catches up
```

### Test 2 - Blocking UART Transmit

Purpose:

```txt
Compare the DMA logger with blocking UART transmit using HAL_UART_Transmit().
```

Expected result:

```txt
[OK] The CPU is stalled during the blocking transmit loop
[OK] LED heartbeat and periodic main log are delayed during the blocking burst
[OK] The final tick value shows how long the blocking burst took
```

This test is intentionally implemented as a comparison case.  
It does not use the ring buffer logger for the burst itself.

### Test 3 - Spam Log

Purpose:

```txt
Verify that many log messages can be queued without blocking the main firmware flow.
```

Expected result:

```txt
[OK] Firmware does not hang
[OK] UART DMA continues transmitting queued logs
[OK] Main loop continues running
[OK] dropped counter increases only if the ring buffer becomes full
```

### Test 4 - Buffer Full

Purpose:

```txt
Verify logger behavior when the ring buffer does not have enough free space.
```

Expected result:

```txt
[OK] Logger does not block
[OK] Logger does not crash
[OK] Logger drops complete messages
[OK] dropped counter increases
```

## Drop Policy

This logger uses a simple drop policy:

```txt
If the ring buffer does not have enough free space for a complete message,
the whole message is dropped and the dropped counter is incremented.
```

The logger does not silently overwrite old data.

If `HAL_UART_Transmit_DMA()` fails to start, the ring buffer tail is not advanced.  
The data remains queued and may be retried by a later transfer start attempt.

## Evidence

Recommended evidence files:

```txt
assets/reports/01_uart_dma_ring_logger.pdf
assets/reports/01_uart_dma_ring_logger.txt

assets/screenshots/01_uart_dma_ring_logger_build_success.png
assets/screenshots/01_uart_dma_ring_logger_normal_log.png
assets/screenshots/01_uart_dma_ring_logger_blocking_test.png
assets/screenshots/01_uart_dma_ring_logger_spam_test.png
assets/screenshots/01_uart_dma_ring_logger_buffer_full.png

assets/logs/01_uart_dma_ring_logger_normal_log.txt
assets/logs/01_uart_dma_ring_logger_blocking_test.txt
assets/logs/01_uart_dma_ring_logger_spam_test.txt
assets/logs/01_uart_dma_ring_logger_buffer_full.txt
```

Optional analysis note:

```txt
assets/logs/01_uart_dma_ring_logger_spam_analysis.md
```

## Related Blog Article

This project is the companion implementation for the STM32 UART DMA logger blog article.

The blog explains the design problem and debugging points.  
This repository project provides the buildable STM32CubeIDE implementation and test evidence.

## Notes for Later Blog Update

The current project is intentionally limited to a bare-metal UART DMA logger.

RTOS-specific topics such as mutex-protected logging, ISR-safe logging, and a dedicated logger task should be moved to the FreeRTOS firmware architecture project.

## Next Project

- `02_freertos_firmware_skeleton`
