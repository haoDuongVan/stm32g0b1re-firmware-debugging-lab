# Project 05 — STM32 USB HID 4x4 Macro Keypad

## 1. Project Goal

Project 05 implements a USB HID keyboard / macro keypad firmware on STM32G0B1RE using a 4x4 matrix keypad module as the input device.

The firmware scans the keypad matrix, debounces key states, detects key on / key off / repeat / simultaneous-key error, converts key events into USB HID boot keyboard reports, and sends those reports to the host PC through USB HID.

This project is not intended to be a full mechanical keyboard or NKRO keyboard. The goal is to build a small but realistic embedded firmware design with product-like input scanning, error handling, HID report conversion, and USB callback sequencing.

---

## 2. Target Board

```txt
Board       : NUCLEO-G0B1RE
MCU         : STM32G0B1RE
IDE         : STM32CubeIDE / CubeMX
USB class   : USB HID Device
Input       : 4x4 matrix keypad module
```

Important board notes:

- The onboard Micro-B connector `CN2` is the ST-LINK USB connector used for programming, debugging, Virtual COM Port, and board power.
- `CN2` is not connected to the USB peripheral pins of the STM32G0B1RE target MCU.
- To enumerate the target MCU as a USB HID keyboard, connect a separate USB cable or breakout to the target MCU USB pins:
  - `PA11` = USB DM / D-
  - `PA12` = USB DP / D+
  - `GND`  = USB ground
- During initial bring-up, power the board from ST-LINK `CN2` and connect only D-, D+, and GND for the HID cable. Do not connect USB VBUS/5V initially.

```txt
USB-A cable / breakout       NUCLEO-G0B1RE target MCU
-----------------------------------------------------
D-                            PA11 / USB_DM
D+                            PA12 / USB_DP
GND                           GND
5V / VBUS                     Not connected initially
```

Configure USB as self-powered in CubeMX when the board is powered from ST-LINK while VBUS is not connected.

---

## 3. CubeMX Configuration

### USB

```txt
USB peripheral:
  USB_DRD_FS enabled
  PA11 = USB_DM
  PA12 = USB_DP

USB_DEVICE middleware:
  Class For FS IP = Human Interface Device Class (HID)
```

### Device Descriptor

```txt
VID                       = 0x0483  (1155 decimal)
PID                       = 0x572B  (22315 decimal)
MANUFACTURER_STRING       = Hao Embedded Lab
PRODUCT_STRING            = STM32 USB HID 4x4 Macro Keypad
CONFIGURATION_STRING      = HID Config
INTERFACE_STRING          = HID Interface
LANGID_STRING             = English (United States)
```

### HID Parameters

```txt
HID_FS_BINTERVAL          = 0x0A    (10 ms polling interval)
USBD_MAX_NUM_INTERFACES   = 1
USBD_MAX_NUM_CONFIGURATION= 1
USBD_MAX_STR_DESC_SIZ     = 512 bytes
USBD_DEBUG_LEVEL          = 0
USBD_SELF_POWERED         = Enabled
USBD_LPM_ENABLED          = Disabled
VBUS sensing              = Disabled
```

### TIM6

TIM6 is used to generate a 5 ms scan request tick. Enable TIM6 NVIC interrupt in CubeMX.

Actual configuration used in this project (TIM6 input clock = 48 MHz):

```txt
Prescaler    = 48 - 1   →  timer tick = 1 MHz
Period       = 5000 - 1 →  update event every 5 ms
```

For a different system clock, recalculate:

```txt
Prescaler = (TIM6 input clock / 1,000,000) - 1
Period    = 5000 - 1
```

The generated `MX_TIM6_Init()` and `htim6` handle are used by `HID_Keyboard_Init()` via `HAL_TIM_Base_Start_IT(&htim6)`.

---

## 4. Matrix GPIO Assignment

All matrix lines are assigned to GPIOB for fast register-based scanning.

```txt
Rows    : PB0 PB1 PB2 PB3
Columns : PB4 PB5 PB6 PB7
```

```txt
Matrix line    STM32 GPIO    Board pin / label
-----------------------------------------------
ROW0           PB0           Pin 27 / ARD_D10
ROW1           PB1           Pin 28 / ARD_A3
ROW2           PB2           Pin 29 / IO
ROW3           PB3           Pin 57 / ARD_D3

COL0           PB4           Pin 58 / ARD_D5
COL1           PB5           Pin 59 / ARD_D4
COL2           PB6           Pin 60 / IO
COL3           PB7           Pin 61 / IO
```

Reserved / avoided pins:

```txt
PA11 / PA12 : USB target D- / D+
PA13 / PA14 : SWDIO / SWCLK
PA2  / PA3  : ST-LINK Virtual COM UART2
PA5         : On-board LED
PC13        : User button
PC14 / PC15 : LSE oscillator pins
PF0  / PF1  : HSE oscillator pins
```

---

## 5. CubeMX GPIO Setup

### Row pins — GPIO Output

```txt
GPIO    User Label      Mode           Output Level    Pull       Speed
-----------------------------------------------------------------------
PB0     MATRIX_ROW0     GPIO_Output    High            No pull    Low
PB1     MATRIX_ROW1     GPIO_Output    High            No pull    Low
PB2     MATRIX_ROW2     GPIO_Output    High            No pull    Low
PB3     MATRIX_ROW3     GPIO_Output    High            No pull    Low
```

### Column pins — GPIO Input

```txt
GPIO    User Label      Mode          Pull-up/Pull-down
-------------------------------------------------------
PB4     MATRIX_COL0     GPIO_Input    Pull-up
PB5     MATRIX_COL1     GPIO_Input    Pull-up
PB6     MATRIX_COL2     GPIO_Input    Pull-up
PB7     MATRIX_COL3     GPIO_Input    Pull-up
```

Scan polarity:

```txt
Row inactive = High
Row active   = Low
Column idle  = High (pull-up)
Key pressed  = Column reads Low
```

---

## 6. 4x4 Keymap

```txt
Row 0:  1       2       3          4
Row 1:  a       b       c          d
Row 2:  Enter   Space   Backspace  Tab
Row 3:  Ctrl+C  Ctrl+V  Ctrl+S     Alt+Tab
```

Physical key location:

```c
keyLoc = row * 4 + col   // range: 0..15
```

Examples:

```txt
Row 0 Col 0 → keyLoc  0 → 1
Row 1 Col 0 → keyLoc  4 → a
Row 2 Col 0 → keyLoc  8 → Enter
Row 3 Col 0 → keyLoc 12 → Ctrl+C
Row 3 Col 3 → keyLoc 15 → Alt+Tab
```

The scan layer stores only `keyLoc`. It does not store ASCII or HID usages.

---

## 7. USB HID Report Format

Standard USB HID boot keyboard 8-byte report:

```txt
Byte 0 : Modifier
Byte 1 : Reserved (always 0x00)
Byte 2 : Keycode 1
Byte 3 : Keycode 2
Byte 4 : Keycode 3
Byte 5 : Keycode 4
Byte 6 : Keycode 5
Byte 7 : Keycode 6
```

Report examples:

```txt
a           : 00 00 04 00 00 00 00 00
1           : 00 00 1E 00 00 00 00 00
Enter       : 00 00 28 00 00 00 00 00
Ctrl+C      : 01 00 06 00 00 00 00 00
Alt only    : 04 00 00 00 00 00 00 00
Alt+Tab     : 04 00 2B 00 00 00 00 00
ErrorRollOver: 00 00 01 01 01 01 01 01
Null report : 00 00 00 00 00 00 00 00
```

This firmware uses a single-key boot keyboard report. Bytes 3–7 are always 0x00.

---

## 8. Module Structure

```txt
Core/Src/main.c                 — CubeMX generated; only HID_Keyboard_Init / HID_Keyboard_App
Core/Src/hid_keyboard_app.c     — transport layer + app entry points + TIM6 callback
Core/Src/hid_keyboard_convert.c — key event → HID report sequence
Core/Src/hid_keyboard_report.c  — 8-byte report builder
Core/Src/key_detect.c           — debounce + key on/off/repeat + simultaneous error
Core/Src/key_event_queue.c      — circular queue of key events
Core/Src/key_table.c            — keyLoc → HID usage / macro / repeat policy
Core/Src/matrix_scan.c          — register-based GPIO matrix scan
Core/Src/scan_scheduler.c       — 5 ms timer request counter

Core/Inc/hid_keyboard_app.h
Core/Inc/hid_keyboard_convert.h
Core/Inc/hid_keyboard_report.h
Core/Inc/key_detect.h
Core/Inc/key_event_queue.h
Core/Inc/key_table.h
Core/Inc/matrix_scan.h
Core/Inc/scan_scheduler.h
```

Module roles:

```txt
matrix_scan.c
  GPIOB register-based matrix scan (BSRR + IDR, no HAL GPIO calls)
  Returns a 16-bit raw state word; bit N = keyLoc N pressed
  MatrixScan_CountPressed() for simultaneous-key detection

scan_scheduler.c
  Incremented every 5 ms by TIM6 period-elapsed ISR
  main loop drains with ScanScheduler_TakeRequest()
  Counter capped at 10 to absorb temporary main loop delays

key_detect.c
  4-sample shift register debounce
  Strict simultaneous-key latch (releases all keys on 2+ press, waits for full release)
  Per-key repeat tick counter; KEY_EVENT_REPEAT every 200 ms
  Detection order: CheckKeyOff → CheckRepeat → CheckKeyOn

key_event_queue.c
  Circular queue (32 entries) of KeyEvent_t {type, keyLoc}
  Peek-then-Pop pattern: convert pops only after send is accepted

key_table.c
  16-entry const table; keyLoc → {kind, modifier, usage, macroId, repeatEnable}
  Normal keys: repeatEnable = 1
  Macro keys:  repeatEnable = 0, kind = KEY_KIND_MACRO

hid_keyboard_report.c
  Builds 8-byte boot keyboard report struct
  SetKey / Clear / SetErrorRollOver / GetData

hid_keyboard_convert.c
  Priority: null report → macro sequence → queue event
  Tap-style output: every ON/REPEAT sends key-down then null report
  Macro sequences driven by gMacroActive / gMacroId / gMacroStep
  KEY_EVENT_OFF is dropped (key state tracked in key_detect.c)
  KEY_EVENT_ERROR (KEY_LOC_ERROR_ROLLOVER) sends ErrorRollOver then null

hid_keyboard_app.c
  UsbHidTransport_* — TX state machine, critical section on Cortex-M0+
  HAL_TIM_PeriodElapsedCallback — forwards TIM6 tick to scan_scheduler
  HID_Keyboard_Init — inits all subsystems, starts TIM6
  HID_Keyboard_App — scan/detect loop + convert run (no HAL_Delay)
```

---

## 9. main.c Convention

All application logic is contained in `HID_Keyboard_Init()` and `HID_Keyboard_App()`. The `main.c` user code sections contain only:

```c
/* USER CODE BEGIN 2 */
HID_Keyboard_Init();
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  HID_Keyboard_App();
}
/* USER CODE END 3 */
```

---

## 10. Scan and Debounce

TIM6 fires every 5 ms and increments `gScanRequestCount` (capped at 10). The main loop drains all pending ticks:

```c
while (ScanScheduler_TakeRequest() != 0U)
{
    KeyDetect_Run();
}
HidKeyboardConvert_Run();
```

`HidKeyboardConvert_Run()` runs outside the scan loop so USB reports are sent immediately when the endpoint becomes idle, without waiting for the next 5 ms tick.

`KeyDetect_Run()` maintains a 4-slot scan history but the current debounce decision uses only the 2 newest samples:

```txt
stable ON  = scanBuffer[0] & scanBuffer[1]    (2 of 2 newest agree pressed)
stable OFF = ~(scanBuffer[0] | scanBuffer[1]) (2 of 2 newest agree released)
```

With a 5 ms scan period, debounce reacts in ~10 ms. The older slots (`scanBuffer[2]`, `scanBuffer[3]`) are kept as reserved history for optional stricter debounce policies.

---

## 11. Key Detection Order

Detection order is fixed inside `KeyDetect_Run()`:

```txt
1. CheckKeyOff   — released keys must be cleared before repeat is checked
2. CheckRepeat   — only keys already in pressed state can repeat
3. CheckKeyOn    — a newly detected key must not repeat in the same cycle
```

---

## 12. Simultaneous Key Error Policy

The 4x4 matrix has no per-key diode. Strict all-release latch is used:

```txt
Raw scan detects >= 2 keys pressed:
  → push KEY_EVENT_ERROR (KEY_LOC_ERROR_ROLLOVER) to queue
  → cancel all currently held keys (push KEY_EVENT_OFF for each)
  → reset scan buffer and repeat counters
  → set gSimultaneousErrorActive = true
  → skip all normal detection

While gSimultaneousErrorActive:
  → ignore all key input regardless of how many keys are pressed
  → exit only when rawState == 0 (every key physically released)
  → then reset scan buffer, repeat counters, key status
```

This means: holding A, accidentally touching B, then releasing B still keeps the error latch active until both A and B are released. A single remaining key after releasing one finger does not resume detection.

---

## 13. Repeat Design

Repeat state is stored per `keyLoc` as a 16-entry tick counter array:

```c
static uint16_t gRepeatTick[MATRIX_KEY_NUM];
```

Constants:

```c
#define KEY_DETECT_SCAN_PERIOD_MS   5U
#define KEY_REPEAT_INTERVAL_MS      200U
#define KEY_REPEAT_INTERVAL_TICKS   (KEY_REPEAT_INTERVAL_MS / KEY_DETECT_SCAN_PERIOD_MS)  // = 40
```

A `KEY_EVENT_REPEAT` is generated every 200 ms for a key that is:

- In `gKeyStatus` (confirmed pressed)
- Still stable ON in the current debounce result
- `repeatEnable = 1` in the key table
- `kind = KEY_KIND_NORMAL`

Macro keys (`KEY_KIND_MACRO`) have `repeatEnable = 0` and are excluded from repeat.

The tick counter resets when a key is first pressed, released, or during simultaneous error recovery.

---

## 14. HID Convert Output Policy

`hid_keyboard_convert.c` uses tap-style output to prevent OS typematic repeat:

```txt
KEY_EVENT_ON / KEY_EVENT_REPEAT:
  Step 0: send key-down report
  Step 1: send null report   ← gNeedNullReport flag

KEY_EVENT_OFF:
  Dropped here (key state already updated in key_detect.c)

KEY_EVENT_ERROR (KEY_LOC_ERROR_ROLLOVER):
  Step 0: send ErrorRollOver report (00 00 01 01 01 01 01 01)
  Step 1: send null report
```

Each step waits for the USB endpoint to become idle before sending. The null report reuses `gNeedNullReport` in all cases.

---

## 15. Macro Sequences

Macro steps are built by `HidKeyboardConvert_BuildMacroReport()`:

```txt
Ctrl+C   (MACRO_CTRL_C):
  Step 0: 01 00 06 00 00 00 00 00
  Step 1: 00 00 00 00 00 00 00 00

Ctrl+V   (MACRO_CTRL_V):
  Step 0: 01 00 19 00 00 00 00 00
  Step 1: 00 00 00 00 00 00 00 00

Ctrl+S   (MACRO_CTRL_S):
  Step 0: 01 00 16 00 00 00 00 00
  Step 1: 00 00 00 00 00 00 00 00

Alt+Tab  (MACRO_ALT_TAB):
  Step 0: 04 00 00 00 00 00 00 00   (Alt only)
  Step 1: 04 00 2B 00 00 00 00 00   (Alt + Tab)
  Step 2: 00 00 00 00 00 00 00 00   (null)
```

Macro state is tracked by three variables in `hid_keyboard_convert.c`:

```c
static bool      gMacroActive;
static MacroId_t gMacroId;
static uint8_t   gMacroStep;
```

When a macro event is accepted, the event is popped from the queue and the macro state machine owns the sequence from that point. The first step is attempted immediately, and if the USB endpoint is not ready to accept a report, the active macro state remains pending and is retried on the next USB-idle cycle. The sequence advances one step per accepted USB report.

---

## 16. USB Transport Layer

The transport layer is implemented in `hid_keyboard_app.c`:

```c
typedef enum { USB_HID_TX_IDLE = 0, USB_HID_TX_BUSY } UsbHidTxState_t;

static volatile UsbHidTxState_t gHidTxState;
```

`UsbHidTransport_SendReport()` uses a short PRIMASK-based critical section for atomic check-and-set. On Cortex-M0+, BASEPRI is not available, so PRIMASK is the simplest option for this short critical section.

The `DataIn` callback in `usbd_hid.c` calls `UsbHidTransport_TxCpltCallback()` through a `__weak` symbol, which resets the TX state to `USB_HID_TX_IDLE`.

---

## 17. Key Event Queue

`key_event_queue.c` implements a 32-entry circular queue:

```c
typedef enum { KEY_EVENT_ON, KEY_EVENT_OFF, KEY_EVENT_REPEAT, KEY_EVENT_ERROR } KeyEventType_t;

typedef struct { KeyEventType_t type; uint8_t keyLoc; } KeyEvent_t;

#define KEY_EVENT_QUEUE_SIZE    32U
#define KEY_LOC_ERROR_ROLLOVER  0xFEU
#define KEY_LOC_ALL_OFF         0xFFU
```

For normal key events and ErrorRollOver events, the convert layer uses a peek-then-pop pattern: the event is peeked first, a report is built and sent, and the event is popped only after `UsbHidTransport_SendReport()` returns `true`. This ensures no event is silently lost when the USB endpoint is temporarily busy.

For macro events, the queue event is popped after the converter accepts ownership of the macro. The remaining sequence is then tracked by `gMacroActive`, `gMacroId`, and `gMacroStep`, so the macro can continue across multiple USB-idle cycles without keeping the original event in the queue.

---

## 18. Key Table

`key_table.c` maps each of the 16 `keyLoc` values to a `KeyTableEntry_t`:

```c
typedef struct {
  KeyKind_t  kind;
  uint8_t    modifier;
  uint8_t    usage;
  MacroId_t  macroId;
  uint8_t    repeatEnable;
} KeyTableEntry_t;
```

```txt
keyLoc  Key          kind           modifier  usage   macroId       repeatEnable
--------------------------------------------------------------------------------
0       1            KEY_KIND_NORMAL  0x00    0x1E    MACRO_NONE    1
1       2            KEY_KIND_NORMAL  0x00    0x1F    MACRO_NONE    1
2       3            KEY_KIND_NORMAL  0x00    0x20    MACRO_NONE    1
3       4            KEY_KIND_NORMAL  0x00    0x21    MACRO_NONE    1
4       a            KEY_KIND_NORMAL  0x00    0x04    MACRO_NONE    1
5       b            KEY_KIND_NORMAL  0x00    0x05    MACRO_NONE    1
6       c            KEY_KIND_NORMAL  0x00    0x06    MACRO_NONE    1
7       d            KEY_KIND_NORMAL  0x00    0x07    MACRO_NONE    1
8       Enter        KEY_KIND_NORMAL  0x00    0x28    MACRO_NONE    1
9       Space        KEY_KIND_NORMAL  0x00    0x2C    MACRO_NONE    1
10      Backspace    KEY_KIND_NORMAL  0x00    0x2A    MACRO_NONE    1
11      Tab          KEY_KIND_NORMAL  0x00    0x2B    MACRO_NONE    1
12      Ctrl+C       KEY_KIND_MACRO   0x00    0x00    MACRO_CTRL_C  0
13      Ctrl+V       KEY_KIND_MACRO   0x00    0x00    MACRO_CTRL_V  0
14      Ctrl+S       KEY_KIND_MACRO   0x00    0x00    MACRO_CTRL_S  0
15      Alt+Tab      KEY_KIND_MACRO   0x00    0x00    MACRO_ALT_TAB 0
```

---

## 19. Bring-Up Milestones

```txt
M1:  Generate USB_DEVICE HID project; confirm build.
M2:  Wire PA11/PA12/GND to external USB and confirm enumeration on host.
M3:  USB HID transport state layer (IDLE/BUSY) in hid_keyboard_app.c.
     DataIn weak callback in usbd_hid.c.
M4:  Key table (key_table.h/.c) and HID report builder (hid_keyboard_report.h/.c).
M5:  Key event queue (key_event_queue.h/.c) and HID convert layer (hid_keyboard_convert.h/.c).
     Fixed test events; confirm 'a' key-down and null report reach host.
M6:  Register-based matrix scan (matrix_scan.h/.c) on PB0-PB7.
     Raw scan test for keyLoc 4 (a) using GPIOB->BSRR and GPIOB->IDR.
M7:  Scan buffers + 2-of-2 debounce + key on/off detection (key_detect.h/.c).
     Remove all test code; firmware detects real key presses.
M8:  5 ms TIM6 scan request counter (scan_scheduler.h/.c).
     Replace HAL_Delay(5) with timer-driven ScanScheduler_TakeRequest() loop.
M9:  Simultaneous key detection + strict all-release latch + ErrorRollOver report.
M10: Macro key sequences: Ctrl+C, Ctrl+V, Ctrl+S, Alt+Tab.
M11: Firmware repeat: KEY_EVENT_REPEAT every 200 ms per key.
     Normal keys only; macro keys excluded by key table policy.
```

---

## 20. Evidence Assets

Evidence files for this project are stored under the project-specific `assets` folders.

```txt
assets/diagrams/05_usb_hid_keyboard/
  wiring-diagram.drawio
  wiring-diagram.svg

assets/screenshots/05_usb_hid_keyboard/
  prototype-wiring.png
  usbview-hid-descriptor.png
  wireshark-usb-enumeration-hid-descriptors.png
  wireshark-usb-hid-report-when-press-1.png
  wireshark-usb-hid-null-report.png
  wireshark-usb-hid-report-when-press-Alt-Tab.png
  wireshark-usb-hid-rollover-report.png

assets/logs/05_usb_hid_keyboard/
  05_usb_hid_keyboard_WireShark_enum_reports_macros.txt
  05_usb_hid_keyboard_usvView_device_descriptor.txt

assets/reports/05_usb_hid_keyboard/
  05_usb_hid_keyboard.pdf
  05_usb_hid_keyboard.txt
```

Evidence purpose:

```txt
wiring-diagram.svg
  Shows the intended wiring between the USB breakout, keypad module, and NUCLEO-G0B1RE.

prototype-wiring.png
  Shows the real breadboard setup used during bring-up.

usbview-hid-descriptor.png
  Confirms the device enumerates as a USB HID keyboard and exposes the expected HID interface / endpoint.

wireshark-usb-enumeration-hid-descriptors.png
  Shows the host reading Device, Configuration, HID, and HID Report descriptors.

wireshark-usb-hid-report-when-press-1.png
wireshark-usb-hid-null-report.png
  Confirm key-down followed by null report.

wireshark-usb-hid-report-when-press-Alt-Tab.png
  Confirms the Alt+Tab macro sequence.

wireshark-usb-hid-rollover-report.png
  Confirms the ErrorRollOver report for simultaneous-key error handling.
```

### USBPcap capture scope

Wireshark USBPcap captures USB transfers at the Windows host / URB level. It is useful for confirming descriptor requests and HID report payloads, but it is not a replacement for a hardware USB protocol analyzer.

This project uses USBPcap evidence to confirm:

```txt
- GET_DESCRIPTOR / SET_CONFIGURATION / SET_IDLE
- HID Report Descriptor request
- Interrupt IN endpoint 0x81
- 8-byte HID keyboard reports
- key-down followed by null report
- macro sequences
- ErrorRollOver report
```

USBPcap evidence is not used to analyze low-level bus packets such as SOF, ACK, NAK, STALL timing, or DATA0 / DATA1 toggle behavior.

---

## 21. Current Limitations

```txt
- Board is powered through ST-LINK CN2 during bring-up.
- Target USB breakout currently uses D-, D+, and GND only.
- USB VBUS / 5V from the target USB cable is not connected in this project setup.
- Keymap is fixed at build time.
- Matrix keypad has no per-key diode, so firmware uses a single-key / all-release error policy instead of NKRO.
- ErrorRollOver has no LED or buzzer feedback in this project.
- Evidence was captured on Windows 11.
```
