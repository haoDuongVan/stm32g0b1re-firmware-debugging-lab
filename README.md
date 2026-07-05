# stm32g0b1re-firmware-debugging-lab

Firmware lab on **NUCLEO-G0B1RE** (STM32G0B1RE, Cortex-M0+) — seven independent projects covering bare-metal bring-up through USB composite device firmware with a Python debug tool.

**Board:** NUCLEO-G0B1RE  
**MCU:** STM32G0B1RE (Cortex-M0+, 64 KB RAM, 512 KB Flash, dual-bank)  
**IDE:** STM32CubeIDE 1.16 / CubeMX  
**Blog:** [duonghao.dev](https://duonghao.dev)

---

## Projects

| # | Name | Description | Status |
|---|------|-------------|--------|
| 00 | [Base Bring-up](#project-00--base-bring-up) | Clock, GPIO, UART validation | Complete |
| 01 | [UART DMA Ring Logger](#project-01--uart-dma-ring-logger) | Non-blocking logger with DMA + ring buffer | Complete |
| 02 | [FreeRTOS Firmware Skeleton](#project-02--freertos-firmware-skeleton) | Task, queue, event skeleton | Complete |
| 03 | [Flash EEPROM Emulation](#project-03--flash-eeprom-emulation) | Key-value store on Flash, page transfer, fault injection | Complete |
| 04 | [UART OTA Bootloader](#project-04--uart-ota-bootloader) | Dual-slot OTA over UART, CRC, automatic rollback | Complete |
| 05 | [USB HID 4×4 Macro Keypad](#project-05--usb-hid-4x4-macro-keypad) | Matrix scan, debounce, HID keyboard + macro sequences | Complete |
| 06 | [USB Composite CDC+HID+Vendor](#project-06--usb-composite-cdchidvendor) | Composite device, CDC log, EP0 vendor control, Bulk IN dump, Python GUI | Complete |

---

## Project 00 — Base Bring-up

`firmware/00_base_bringup`

Validates the development environment before any real firmware work: 64 MHz clock, GPIO, UART2 output through the ST-LINK VCP.

**Evidence:** UART log confirming `[BOOT] OK`, build success screenshot.

---

## Project 01 — UART DMA Ring Logger

`firmware/01_uart_dma_ring_logger`

Non-blocking UART logger using DMA and a ring buffer. No `HAL_UART_Transmit` blocking calls anywhere in the application path.

**Technical highlights:**
- 256-byte ring buffer with head/tail indices, DMA-safe
- TX triggered only when the buffer has data and DMA is idle
- Four test modes: normal log, buffer-full drop, blocking comparison, spam burst

**Evidence:** 4 captured logs covering each test scenario.

---

## Project 02 — FreeRTOS Firmware Skeleton

`firmware/02_freertos_firmware_skeleton`

FreeRTOS skeleton with a clean task structure and queue stress test — intended as a reusable starting point for more complex firmware.

**Technical highlights:**
- Three tasks: SensorTask, ProcessTask, LogTask
- `AppEventQueue` producer/consumer pattern
- Event timeout and error handling
- UART logging from both interrupt-safe and task contexts

**Evidence:** Normal-operation log, queue stress test log.

---

## Project 03 — Flash EEPROM Emulation

`firmware/03_flash_eeprom_emulation`

Key-value store implemented directly on STM32G0B1RE internal Flash — no external EEPROM required.

**Technical highlights:**
- Two Flash pages used as alternating active/backup slots (simple wear levelling)
- Write: append-only; Read: scan backward from end of page
- Page transfer triggered when the active page fills up
- Fault injection tests simulate power-loss at different write phases

**Evidence:** 11 test cases with UART logs and screenshots (boot check, write/readback, page transfer, fault recovery).

---

## Project 04 — UART OTA Bootloader

`firmware/04_uart_bootloader`

Dual-slot OTA bootloader over UART with CRC verification and automatic rollback after repeated boot failures.

**Flash layout:**
```
0x0800_0000  Bootloader    (32 KB)
0x0800_8000  Slot A — App  (120 KB)
0x0802_6000  Slot B — App  (120 KB)
0x0804_4000  Metadata      (2 KB)
```

**Technical highlights:**
- Metadata page: pending slot, boot count, confirmed flag, CRC32
- Automatic rollback to Slot A after 3 consecutive boot failures
- Python tool `uart_packet_sender.py` sends binary images over UART
- Vector table relocation via `SCB->VTOR` before jumping to application

**Evidence:** Boot log, OTA transfer log, rollback sequence log, linker map file verification.

---

## Project 05 — USB HID 4×4 Macro Keypad

`firmware/05_usb_hid_keyboard`

USB HID keyboard firmware using a 4×4 matrix keypad on GPIOB. GPIO scan uses direct register access (BSRR + IDR) instead of HAL_GPIO.

**8-module pipeline:**
```
scan_scheduler → matrix_scan → key_detect → key_event_queue
→ hid_keyboard_convert → hid_keyboard_report → hid_keyboard_app → usbd_hid
```

**Technical highlights:**
- Register-based GPIO scan: BSRR set/reset, IDR read — no HAL_GPIO overhead
- TIM6 5 ms tick drives the scan loop; no HAL_Delay in the main loop
- 2-of-2 debounce over a 4-sample rolling buffer
- All-release latch policy for simultaneous-key errors (matrix has no per-key diodes)
- Tap-style HID output: key-down immediately followed by null report
- Macro sequences: Ctrl+C, Ctrl+V, Ctrl+S, Alt+Tab (3-step sequence)
- PRIMASK-based critical section on Cortex-M0+ (no BASEPRI available)

**Wireshark evidence (87 frames):**
- Frame 1–12: Enumeration (Device, Configuration, HID Report descriptors; SET_IDLE)
- Frame 15–61: 12 keys pressed sequentially: 1 2 3 4 a b c d Enter Space Backspace Tab
- Frame 63–79: 4 macro sequences (Ctrl+C/V/S, Alt+Tab)
- Frame 85–87: ErrorRollOver triggered by pressing two keys simultaneously

---

## Project 06 — USB Composite CDC+HID+Vendor

`firmware/06_usb_composite_cdc_hid_vendor`

USB composite device exposing three simultaneous communication channels over a single USB cable: HID keyboard input, CDC firmware log stream, and a vendor debug/control channel.

**Interface map:**
```
IF0 — HID Keyboard       EP1 IN  interrupt  8 B   keyboard HID report
IF1 — CDC Communication  EP2 IN  interrupt  8 B   CDC ACM notification
IF2 — CDC Data           EP3 OUT/IN  bulk  64 B   realtime firmware log
IF3 — Vendor Bulk Debug  EP4 IN  bulk      64 B   RAM dump stream
EP0 — Control                                      vendor-specific requests
```

**Device descriptor:**
```
bDeviceClass:    0xEF  (Multi-interface Function Code Device)
bDeviceSubClass: 0x02  (Common Class)
bDeviceProtocol: 0x01  (Interface Association Descriptor)
VID / PID:       0x0483 / 0x572B
Product string:  "STM32 USB Composite Keypad+CDC+Bulk"
```

**Technical highlights:**
- Composite Configuration Descriptor (116 bytes) written by hand — CubeMX wizard does not generate IAD or multi-class composites correctly
- IAD wraps IF1+IF2 as a single CDC ACM function per USB spec
- Full HID keyboard engine reused unchanged from Project 05
- CDC log path (`CdcLog_Printf`) is non-blocking; dropped bytes are counted, not blocking
- EP0 vendor protocol: 4 commands with typed response structs (`FirmwareInfo_t` 20 B, `VendorResponse_t` 2 B, `VendorDumpResponse_t` 6 B)
- Vendor Bulk IN streams full SRAM1 (144 KB) driven by DataIn callback; measured throughput ~421 KB/s on USB Full Speed
- Vendor interface (IF3) requires Zadig + libusbK on Windows — HID and CDC use inbox drivers

**EP0 vendor command table:**

| bRequest | Command | wValue | Response |
|---:|--------|--------|----------|
| 0x01 | `GET_FIRMWARE_INFO` | — | `FirmwareInfo_t` (20 B) |
| 0x02 | `SET_REPEAT_ENABLE` | 0 = off, 1 = on | `VendorResponse_t` (2 B) |
| 0x03 | `SET_LED_MODE` | 0 off / 1 on / 2 blink-slow / 3 blink-fast | `VendorResponse_t` (2 B) |
| 0x04 | `START_RAM_DUMP` | — | `VendorDumpResponse_t` (6 B) |

**Python debug tool** (`firmware/06_usb_composite_cdc_hid_vendor/tools/composite_debug_tool/`):
- tkinter GUI with CDC log panel (left) and vendor command panel (right)
- Separate threads for CDC serial read and vendor bulk dump
- Dependencies: `pyserial`, `pyusb`, libusb backend (see `requirements.txt`)

**Evidence:**
- USBView: 4 interfaces, 5 open pipes
- Device Manager: HID Keyboard + USB Serial Device (COM port) + Vendor (libusbK)
- Wireshark USBPcap: enumeration with IAD visible in Configuration Descriptor
- TeraTerm: CDC log stream live during operation
- `vendor_test.py`: all 5 tests passed; 144 KB dump in 0.34 s @ 421 KB/s
- Python GUI screenshot

> **Important:** All USB changes were made by hand after copying from Project 05. The `.ioc` file does not reflect the composite configuration. Do **not** click _Generate Code_ in CubeMX without backing up the entire `USB_Device/` folder first. See `firmware/06_usb_composite_cdc_hid_vendor/README.md` for details.

---

## Repository layout

```
firmware/
  00_base_bringup/
  01_uart_dma_ring_logger/
  02_freertos_firmware_skeleton/
  03_flash_eeprom_emulation/
  04_uart_bootloader/
    bootloader/
    app/
    common/
    tools/
  05_usb_hid_keyboard/
  06_usb_composite_cdc_hid_vendor/
    tools/composite_debug_tool/

assets/
  logs/         UART captures, Wireshark text export, USBPcap .pcapng
  reports/      PDF and TXT reports per project
  screenshots/  PNG screenshots per project
  diagrams/     .drawio source diagrams

docs/
  00-board-setup.md
  01-development-workflow.md
  02-debug-checklist.md

tools/
  usbview.exe
  zadig-2.9.exe
```

---

## Board connections

```
ST-LINK CN2  — board power, programming, debugging
               UART2: PA2 TX / PA3 RX → ST-LINK VCP → host USB

Target USB   — separate USB peripheral (Projects 05–06 only)
(Project 05–06)  PA11 = USB_DM / D−
                 PA12 = USB_DP / D+
                 GND  = USB ground
               Connect through a USB breakout board, not through CN2.
```

See `docs/00-board-setup.md` for the full setup guide.

---

## License

Application source code in this repository: **MIT**.

Third-party firmware components (STM32 HAL, CMSIS, FreeRTOS, ST USB Device Library): subject to their respective ST Microelectronics licenses — see `LICENSE.txt` inside each `Drivers/` and `Middlewares/` subdirectory.
