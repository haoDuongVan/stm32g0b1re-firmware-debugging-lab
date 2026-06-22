# 00 Base Bring-up

Minimal firmware project for validating the NUCLEO-G0B1RE development setup before starting the main debugging projects.

## Purpose

This project checks that the board, STM32CubeIDE project, clock configuration, GPIO, UART output, and on-board debugger are working correctly.

It is the baseline project for the rest of the firmware debugging lab.

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
| LED | On-board user LED |

## Expected Behavior

After flashing the firmware:

1. The on-board LED toggles every 1 second.
2. The UART terminal prints a boot message.
3. The UART terminal prints a tick log every 1 second.
4. The firmware can be flashed and debugged through the on-board ST-LINK debugger.

Example output:

```txt
[BOOT] STM32G0B1RE Firmware Debugging Lab
[BOOT] Project: 00_base_bringup
[BOOT] Board: NUCLEO-G0B1RE
[BOOT] System clock: 48 MHz
[BOOT] UART debug initialized
[BOOT] Starting main loop
[MAIN] tick=1000
[MAIN] tick=2000
[MAIN] tick=3000
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

## STM32CubeIDE Setup Notes

Recommended `.ioc` configuration:

```txt
SYS Debug: Serial Wire
USART2 Mode: Asynchronous
System Clock: 48 MHz
GPIO: On-board user LED as output
```

The board is powered, flashed, and debugged through the USB connector connected to the on-board ST-LINK debugger.

No external ST-LINK probe is required.

## Project Scope

This project intentionally keeps the firmware simple.

Included:

- HAL initialization
- GPIO LED toggle
- blocking UART transmit for basic debug output
- basic main loop timing using `HAL_GetTick()`

Not included:

- UART DMA
- ring buffer logger
- FreeRTOS
- Flash storage
- bootloader
- USB device stack

These topics are handled in later projects.

## Test Checklist

The project is complete when the following items are confirmed:

```txt
[OK] STM32CubeIDE build succeeds
[OK] Firmware flashing succeeds
[OK] Debug session starts through on-board ST-LINK
[OK] On-board LED toggles every 1 second
[OK] UART boot log is printed
[OK] UART tick log is printed every 1 second
```

## Evidence

Recommended evidence files:

```txt
assets/reports/00_base_bringup.pdf
assets/reports/00_base_bringup.txt
assets/screenshots/00_base_bringup_build_success.png
assets/screenshots/00_base_bringup_uart_log.png
assets/logs/00_base_bringup_uart_log.txt
```

Optional:

```txt
assets/screenshots/00_base_bringup_debug_session.png
```

## Next Project

- `01_uart_dma_ring_logger`
