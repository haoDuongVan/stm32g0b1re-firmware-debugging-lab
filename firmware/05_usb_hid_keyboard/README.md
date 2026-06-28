# Project 05 — STM32 USB HID 4x4 Macro Keypad

## 1. Project Goal

Project 05 implements a USB HID keyboard / macro keypad firmware on STM32 using a 4x4 matrix keypad module as the input device.

The firmware scans the keypad matrix, debounces key states, detects key on / key off / repeat / simultaneous-key error, converts key events into USB HID boot keyboard reports, and sends those reports directly to the host PC through USB HID.

This project is not intended to be a full mechanical keyboard or NKRO keyboard. The goal is to build a small but realistic embedded firmware design with product-like input scanning, error handling, HID report conversion, and USB callback sequencing.

---

## 2. Current Target Board

Initial target board:

```txt
Board       : NUCLEO-G0B1RE
MCU         : STM32G0B1RE
IDE         : STM32CubeIDE / CubeMX
USB class   : USB HID Device
Input       : 4x4 matrix keypad module
```

Important board notes:

- The onboard Micro-B connector `CN2` is the ST-LINK USB connector.
- `CN2` is used for programming, debugging, ST-LINK Virtual COM Port, and board power.
- `CN2` is not directly connected to the USB peripheral pins of the STM32G0B1RE target MCU.
- To make the target MCU enumerate as a USB HID keyboard, connect a separate USB cable/breakout to the target MCU USB pins:
  - `PA11` = USB DM / D-
  - `PA12` = USB DP / D+
  - `GND`  = USB ground
- During initial bring-up, power the board from ST-LINK `CN2` and connect only `D-`, `D+`, and `GND` for the target USB HID cable. Do not connect USB VBUS/5V at first.

Recommended USB wiring during bring-up:

```txt
USB-A cable / breakout       NUCLEO-G0B1RE target MCU
-----------------------------------------------------
D-                            PA11 / USB_DM / Pin 43
D+                            PA12 / USB_DP / Pin 44
GND                           GND
5V / VBUS                     Not connected initially
```

If the board is powered from ST-LINK while the target USB HID cable only provides D+/D-/GND, configure USB as a self-powered device for this bring-up setup.

---

## 3. Current CubeMX USB Configuration

The current CubeMX configuration is:

```txt
USB peripheral:
  USB_DRD_FS enabled
  PA11 = USB_DM
  PA12 = USB_DP

USB_DEVICE middleware:
  Class For FS IP = Human Interface Device Class (HID)
```

Device descriptor parameters:

```txt
VID                      = 1155 decimal = 0x0483
PID                      = 22315 decimal = 0x572B
LANGID_STRING             = English (United States)
MANUFACTURER_STRING       = Hao Embedded Lab
PRODUCT_STRING            = STM32 USB HID 4x4 Macro Keypad
CONFIGURATION_STRING      = HID Config
INTERFACE_STRING          = HID Interface
```

USB_DEVICE class/basic parameters:

```txt
HID_FS_BINTERVAL          = 0x0A    // 10 ms polling interval
USBD_MAX_NUM_INTERFACES   = 1
USBD_MAX_NUM_CONFIGURATION= 1
USBD_MAX_STR_DESC_SIZ     = 512 bytes
USBD_DEBUG_LEVEL          = 0
```

Bring-up recommendations:

```txt
USBD_SELF_POWERED         = Enabled, if powered from ST-LINK CN2
USBD_LPM_ENABLED          = Disabled for first bring-up
VBUS sensing              = Disabled for first bring-up if VBUS is not connected
```

The current VID/PID is acceptable for local development and personal testing. For a production product, use a properly assigned VID/PID.

---

## 4. Matrix GPIO Assignment for NUCLEO-G0B1RE

For clean and fast register-based scanning, all matrix lines are assigned to GPIOB.

Logical assignment:

```txt
Rows    : PB0 PB1 PB2 PB3
Columns : PB4 PB5 PB6 PB7
```

Board pin mapping from the NUCLEO-G0B1RE I/O assignment:

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

Reason for this pin plan:

- All 8 matrix lines are on GPIOB.
- Row driving can use `GPIOB->BSRR`.
- Column reading can use `GPIOB->IDR`.
- Bit masks are simple and fast.
- USB pins, SWD pins, ST-LINK VCP pins, LED pin, and user button pin are avoided.

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

## 5. CubeMX GPIO Setup for the 4x4 Matrix

Set the following 8 GPIO pins in CubeMX.

### 5.1 Row pins

Rows are output pins. They are normally inactive high. During scanning, one selected row is driven low.

```txt
GPIO    User Label    Mode           Output Level    Pull       Speed
---------------------------------------------------------------------
PB0     MATRIX_ROW0   GPIO_Output    High            No pull    Low
PB1     MATRIX_ROW1   GPIO_Output    High            No pull    Low
PB2     MATRIX_ROW2   GPIO_Output    High            No pull    Low
PB3     MATRIX_ROW3   GPIO_Output    High            No pull    Low
```

Recommended output configuration:

```txt
GPIO mode           : Output Push Pull
GPIO output level   : High
GPIO pull-up/down   : No pull-up and no pull-down
Maximum output speed: Low
```

If oscilloscope measurement shows that the row signal edges are too slow for the selected wiring, change speed to Medium. For a keypad scan at 5 ms period, Low speed should be enough in most cases.

### 5.2 Column pins

Columns are input pins with internal pull-up enabled.

```txt
GPIO    User Label    Mode         Pull-up/Pull-down
----------------------------------------------------
PB4     MATRIX_COL0   GPIO_Input   Pull-up
PB5     MATRIX_COL1   GPIO_Input   Pull-up
PB6     MATRIX_COL2   GPIO_Input   Pull-up
PB7     MATRIX_COL3   GPIO_Input   Pull-up
```

Recommended input configuration:

```txt
GPIO mode           : Input mode
GPIO pull-up/down   : Pull-up
```

Scan polarity:

```txt
Row inactive = High
Row active   = Low
Column idle  = High
Pressed key  = Column reads Low
```

---

## 6. 4x4 Keymap

The 4x4 keypad is mapped as follows:

```txt
1       2       3          4
a       b       c          d
Enter   Space   Backspace  Tab
Ctrl+C  Ctrl+V  Ctrl+S     Alt+Tab
```

Key location rule:

```c
keyLoc = row * 4 + col;
```

Examples:

```txt
Row 0 Col 0 -> keyLoc 0  -> 1
Row 0 Col 1 -> keyLoc 1  -> 2
Row 1 Col 0 -> keyLoc 4  -> a
Row 2 Col 0 -> keyLoc 8  -> Enter
Row 3 Col 0 -> keyLoc 12 -> Ctrl+C
Row 3 Col 3 -> keyLoc 15 -> Alt+Tab
```

The scan layer only stores `keyLoc`. It does not store ASCII characters and does not store HID reports.

---

## 7. USB HID Report Format

Use the standard USB HID boot keyboard report format:

```txt
Byte 0 : Modifier
Byte 1 : Reserved
Byte 2 : Keycode 1
Byte 3 : Keycode 2
Byte 4 : Keycode 3
Byte 5 : Keycode 4
Byte 6 : Keycode 5
Byte 7 : Keycode 6
```

Examples:

```txt
a:
00 00 04 00 00 00 00 00

1:
00 00 1E 00 00 00 00 00

Enter:
00 00 28 00 00 00 00 00

Ctrl+C:
01 00 06 00 00 00 00 00

Alt+Tab:
04 00 2B 00 00 00 00 00

ErrorRollOver:
00 00 01 01 01 01 01 01

Null report:
00 00 00 00 00 00 00 00
```

The ErrorRollOver report is used when the firmware detects an unsafe multi-key condition on the diode-less 4x4 matrix.

---

## 8. GPIO Scan Design

The matrix scan function should not use HAL GPIO calls in the critical scan path.

Use direct GPIO registers:

```txt
GPIOB->BSRR : atomic set/reset row pins
GPIOB->IDR  : read column pins
```

Initial GPIO design:

```txt
Rows:
  output push-pull
  default High
  active Low

Columns:
  input pull-up
  idle High
  pressed Low
```

Scan flow:

```txt
1. Set all rows inactive High.
2. For each row:
   - Drive selected row Low.
   - Wait for GPIO/line settling using NOP.
   - Read all column pins from GPIOB->IDR.
   - Convert column state to raw matrix bitmap.
   - Drive selected row back High.
3. Leave all rows inactive High after scan.
```

The NOP delay after changing a row is required when using direct register access. It does not need to be exposed as a macro in the first version. A few explicit `__NOP()` instructions are enough as a visible placeholder in the scan code.

Example:

```c
__NOP();
__NOP();
__NOP();
__NOP();
```

The exact number of NOPs is a hardware timing parameter. It should be adjusted only after checking the row-to-column settling time with an oscilloscope or logic analyzer.

For measurement, a debug GPIO may be toggled around the settle delay:

```txt
debug pin high
row active
NOP settle delay
read columns
debug pin low
```

This makes the scan timing visible on an oscilloscope.

---

## 9. Scan Timing

A hardware timer runs every 5 ms.

The timer interrupt does not scan the matrix directly. It only increases a scan request counter:

```c
static volatile uint8_t gScanRequestCount;
```

Timer callback:

```c
if (gScanRequestCount < 10U) {
  gScanRequestCount++;
}
```

Main scan-detect loop:

```c
void MainScanDetect_Run(void)
{
  while (gScanRequestCount != 0U) {
    __disable_irq();
    gScanRequestCount--;
    __enable_irq();

    MatrixScanDetect_Run5ms();
  }
}
```

A counter is used instead of a single flag to avoid losing scan ticks when the main loop is temporarily busy.

---

## 10. Scan Buffers and Debounce

Use 4 scan buffers. Each buffer stores one 5 ms matrix scan result.

```c
#define MATRIX_SCAN_BUFFER_COUNT  4U
#define MATRIX_KEY_COUNT          16U
#define MATRIX_KEY_BYTE_COUNT     2U
```

Recommended state storage:

```c
typedef struct {
  uint8_t scanBuffer[MATRIX_SCAN_BUFFER_COUNT][MATRIX_KEY_BYTE_COUNT];
  uint8_t stableKeyState[MATRIX_KEY_BYTE_COUNT];
  uint8_t previousKeyState[MATRIX_KEY_BYTE_COUNT];
} MatrixKeyState_t;
```

Buffer meaning:

```txt
scanBuffer[0] = newest scan
scanBuffer[1] = previous scan
scanBuffer[2] = older scan
scanBuffer[3] = oldest scan

stableKeyState = current stable key state
```

Normal debounce path:

```txt
new_on  = scanBuffer[0] & scanBuffer[1]
new_off = ~(scanBuffer[0] | scanBuffer[1])
```

Meaning:

```txt
2 consecutive ON samples  -> accept ON
2 consecutive OFF samples -> accept OFF
```

With a 5 ms scan period, the normal debounce path reacts in about 10 ms. The recovery path uses all 4 buffers for a stricter check.

---

## 11. Key Detection Order

In the normal path, the detection order is fixed:

```txt
CheckKeyOff()
CheckRepeat()
CheckKeyOn()
```

Reason:

```txt
CheckKeyOff first:
  A released key must be handled before repeat processing.

CheckRepeat second:
  Only keys that remain stable pressed can repeat.

CheckKeyOn last:
  A newly pressed key must not repeat in the same scan cycle.
```

Normal scan-detect flow:

```c
void MatrixScanDetect_Run5ms(void)
{
  ScanMatrix();
  CheckSimultaneousKey();

  if (gSimultaneousNow != 0U) {
    HandleSimultaneousError();
    return;
  }

  if (gSimultaneousError != 0U) {
    ErrorRecoveryCheck();
    return;
  }

  UpdateDebounceState();

  CheckKeyOff();
  CheckRepeat();
  CheckKeyOn();
}
```

---

## 12. Simultaneous Key Error Policy

Because the 4x4 matrix module has no per-key diode, the firmware uses a strict safety policy:

```txt
If one raw scan detects two or more pressed keys:
  set gSimultaneousNow
  set gSimultaneousError
  put KEY_EVENT_ERROR + KEY_LOC_ERROR_ROLLOVER into the ring buffer
  skip CheckKeyOff()
  skip CheckRepeat()
  skip CheckKeyOn()
```

The standard USB HID boot keyboard ErrorRollOver report is:

```txt
00 00 01 01 01 01 01 01
```

After ErrorRollOver, the firmware sends a null report:

```txt
00 00 00 00 00 00 00 00
```

Error sequence:

```txt
Step 0: ErrorRollOver report
Step 1: Null report
```

Ring buffer special key locations:

```c
#define KEY_LOC_ERROR_ROLLOVER  0xFEU
#define KEY_LOC_ALL_OFF         0xFFU
```

The ring buffer does not store 8-byte HID reports. It only stores key event type and key location.

---

## 13. Error Recovery Policy

When the firmware is in simultaneous-key error mode, it does not process the matrix as a normal scan.

Recovery uses a dedicated function:

```c
void ErrorRecoveryCheck(void);
```

Recovery flow:

```txt
If gSimultaneousNow is still active:
  return

Step 1:
  CheckKeyOff()
  CheckRepeat()

Step 2:
  AND all 4 scan buffers to find a stable pressed key

Step 3:
  Compare with stableKeyState to find a new key press

Step 4:
  If a new key press exists:
    apply recovery policy
    if allowed, put keyLoc into the ring buffer
```

Recovery policy:

```txt
Normal key:
  Can be accepted again if stable in all 4 scan buffers.

Macro key:
  Must not auto-fire after a simultaneous-key error.
  The user must release all keys and press the macro key again.
```

Reason: macro keys such as `Ctrl+S`, `Ctrl+V`, and `Alt+Tab` can affect the host PC significantly, so they should not be triggered automatically after an error condition.

---

## 14. Key Status Flags

Use clear, separate flags for current scan state and latched error state.

```c
static uint8_t gSimultaneousNow;
static uint8_t gSimultaneousError;
static uint8_t gRecoveryActive;
```

Meaning:

```txt
gSimultaneousNow:
  The current raw scan sees two or more pressed keys.

gSimultaneousError:
  The firmware is in simultaneous-key error mode and must run recovery.

gRecoveryActive:
  Optional flag for debugging or tracing the recovery path.
```

Do not use a single `Simul` flag for both meanings. If the same flag means both "current scan has error" and "system is in recovery mode", recovery can accidentally return forever.

---

## 15. Key Ring Buffer

The key ring buffer stores key events by key location. It does not store ASCII characters and does not store HID report bytes.

```c
typedef enum {
  KEY_EVENT_ON,
  KEY_EVENT_OFF,
  KEY_EVENT_REPEAT,
  KEY_EVENT_ERROR
} KeyEventType_t;

typedef struct {
  KeyEventType_t type;
  uint8_t keyLoc;
} KeyEvent_t;
```

Examples:

```txt
KEY_EVENT_ON     + keyLoc 4  -> press key a
KEY_EVENT_OFF    + keyLoc 4  -> release key a
KEY_EVENT_REPEAT + keyLoc 4  -> repeat key a
KEY_EVENT_ERROR  + 0xFE      -> ErrorRollOver sequence
KEY_EVENT_OFF    + 0xFF      -> Null / all-off report
```

A struct-based ring buffer is preferred for the first implementation because it is easier to debug and log. A packed 1-byte format can be added later if needed.

---

## 16. Key Table

When the main convert loop gets an event from the ring buffer, it fetches the actual key definition from the key table using `keyLoc`.

The scan layer does not know whether a key is `a`, `Enter`, `Ctrl+C`, or `Alt+Tab`. It only knows the physical key location.

```c
typedef enum {
  KEY_KIND_NORMAL,
  KEY_KIND_MACRO,
  KEY_KIND_SPECIAL
} KeyKind_t;

typedef enum {
  MACRO_NONE,
  MACRO_CTRL_C,
  MACRO_CTRL_V,
  MACRO_CTRL_S,
  MACRO_ALT_TAB
} MacroId_t;

typedef struct {
  KeyKind_t kind;
  uint8_t modifier;
  uint8_t usage;
  MacroId_t macroId;
  uint8_t repeatEnable;
} KeyTableEntry_t;
```

Key table:

```c
static const KeyTableEntry_t gKeyTable[16] = {
  /* Row 0: 1 2 3 4 */
  { KEY_KIND_NORMAL, 0x00U, 0x1EU, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x1FU, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x20U, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x21U, MACRO_NONE,    1U },

  /* Row 1: a b c d */
  { KEY_KIND_NORMAL, 0x00U, 0x04U, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x05U, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x06U, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x07U, MACRO_NONE,    1U },

  /* Row 2: Enter Space Backspace Tab */
  { KEY_KIND_NORMAL, 0x00U, 0x28U, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x2CU, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x2AU, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, 0x00U, 0x2BU, MACRO_NONE,    1U },

  /* Row 3: Ctrl+C Ctrl+V Ctrl+S Alt+Tab */
  { KEY_KIND_MACRO,  0x00U, 0x00U, MACRO_CTRL_C,  0U },
  { KEY_KIND_MACRO,  0x00U, 0x00U, MACRO_CTRL_V,  0U },
  { KEY_KIND_MACRO,  0x00U, 0x00U, MACRO_CTRL_S,  0U },
  { KEY_KIND_MACRO,  0x00U, 0x00U, MACRO_ALT_TAB,0U },
};
```

`repeatEnable` is per key. Normal keys can repeat. Macro keys do not repeat.

---

## 17. Repeat Design

Repeat uses the 5 ms scan timer as the base tick.

Repeat state is stored per key location:

```c
typedef struct {
  uint16_t tickCount;
} KeyRepeatState_t;

static KeyRepeatState_t gRepeatState[16];
```

Repeat interval:

```c
#define MATRIX_SCAN_PERIOD_MS    5U
#define REPEAT_INTERVAL_MS       200U
#define REPEAT_INTERVAL_TICKS    (REPEAT_INTERVAL_MS / MATRIX_SCAN_PERIOD_MS)
```

Meaning:

```txt
If a repeat-enabled key is held continuously,
a KEY_EVENT_REPEAT is generated every 200 ms.
```

Pseudo logic:

```c
static void CheckRepeat(void)
{
  uint8_t keyLoc;

  for (keyLoc = 0U; keyLoc < 16U; keyLoc++) {
    if ((IsKeyStableHeld(keyLoc) != 0U) &&
        (gKeyTable[keyLoc].repeatEnable != 0U)) {

      gRepeatState[keyLoc].tickCount++;

      if (gRepeatState[keyLoc].tickCount >= REPEAT_INTERVAL_TICKS) {
        gRepeatState[keyLoc].tickCount = 0U;
        KeyRingBuffer_Put(KEY_EVENT_REPEAT, keyLoc);
      }
    } else {
      gRepeatState[keyLoc].tickCount = 0U;
    }
  }
}
```

Macro repeat is disabled by key table policy.

---

## 18. Macro Convert Design

Macro keys are not handled in the scan layer.

The scan layer only puts `keyLoc` into the ring buffer. The convert layer does:

```txt
keyLoc -> gKeyTable[keyLoc] -> CheckMacroKey() -> MacroConvert_Start()
```

Required functions:

```c
static uint8_t CheckMacroKey(const KeyTableEntry_t *key);
static uint8_t MacroConvert_Start(MacroId_t macroId, uint8_t report[8]);
static uint8_t MacroConvert_Next(uint8_t report[8]);
```

Macro sequences:

```txt
Ctrl+C:
  Step 0: 01 00 06 00 00 00 00 00
  Step 1: 00 00 00 00 00 00 00 00

Ctrl+V:
  Step 0: 01 00 19 00 00 00 00 00
  Step 1: 00 00 00 00 00 00 00 00

Ctrl+S:
  Step 0: 01 00 16 00 00 00 00 00
  Step 1: 00 00 00 00 00 00 00 00

Alt+Tab:
  Step 0: 04 00 00 00 00 00 00 00
  Step 1: 04 00 2B 00 00 00 00 00
  Step 2: 00 00 00 00 00 00 00 00
```

---

## 19. HID Convert State Machine

The main convert loop starts a new event only when HID transmission is idle.

State definitions:

```c
typedef enum {
  HID_TX_IDLE,
  HID_TX_BUSY
} HidTxState_t;

typedef enum {
  HID_SEQUENCE_NONE,
  HID_SEQUENCE_NORMAL,
  HID_SEQUENCE_MACRO,
  HID_SEQUENCE_ERROR,
  HID_SEQUENCE_NULL
} HidSequenceType_t;

typedef struct {
  HidSequenceType_t type;
  MacroId_t macroId;
  uint8_t step;
  uint8_t active;
} HidSequenceContext_t;
```

Main convert starts a new event only when:

```txt
gHidTxState == HID_TX_IDLE
gHidSequence.active == 0
```

Main convert responsibilities:

```txt
1. Pop key event from ring buffer.
2. Fetch key table by keyLoc.
3. Start normal / macro / error / null sequence.
4. Send the first HID report.
```

USB HID DataIn callback responsibilities:

```txt
1. Do not pop a new key event.
2. Call HidConvert_Next().
3. If the current sequence has another report, send it.
4. If the sequence is complete, clear HID busy state.
```

---

## 20. Race Protection Between Main Convert and DataIn Callback

Because the USB HID DataIn callback can run while the main loop is checking HID state, the main convert loop must use a short critical section when checking and setting HID state.

Pseudo logic:

```c
void MainConvert_Run(void)
{
  uint8_t canStart = 0U;

  __disable_irq();

  if ((gHidTxState == HID_TX_IDLE) && (gHidSequence.active == 0U)) {
    gHidTxState = HID_TX_BUSY;
    gHidSequence.active = 1U;
    canStart = 1U;
  }

  __enable_irq();

  if (canStart == 0U) {
    return;
  }

  /* Pop key event, convert first report, send. */
  /* If no event exists, restore HID state to idle. */
}
```

The callback never takes a new event from the ring buffer. It only continues the current HID sequence.

---

## 21. Main Loop Structure

Final main loop structure:

```c
while (1) {
  MainScanDetect_Run();
  MainConvert_Run();
}
```

`MainScanDetect_Run()` handles pending 5 ms scan requests:

```c
void MainScanDetect_Run(void)
{
  while (gScanRequestCount != 0U) {
    __disable_irq();
    gScanRequestCount--;
    __enable_irq();

    MatrixScanDetect_Run5ms();
  }
}
```

`MainConvert_Run()` handles one HID event if the USB HID transmit state is idle.

The USB DataIn callback handles the remaining reports of the current sequence.

---

## 22. Suggested Module Structure

```txt
Core/Src/main.c
Core/Src/matrix_scan.c
Core/Src/key_detect.c
Core/Src/key_ring_buffer.c
Core/Src/key_table.c
Core/Src/hid_keyboard_convert.c
Core/Src/usb_hid_transport.c

Core/Inc/matrix_scan.h
Core/Inc/key_detect.h
Core/Inc/key_ring_buffer.h
Core/Inc/key_table.h
Core/Inc/hid_keyboard_convert.h
Core/Inc/usb_hid_transport.h
```

Module roles:

```txt
matrix_scan.c:
  GPIOB register-based matrix scan
  scan buffer update
  raw simultaneous-key detection

key_detect.c:
  CheckKeyOff
  CheckRepeat
  CheckKeyOn
  ErrorRecoveryCheck

key_ring_buffer.c:
  key event put/get

key_table.c:
  keyLoc -> HID usage / macro id / repeat policy

hid_keyboard_convert.c:
  key event -> HID report sequence

usb_hid_transport.c:
  USB HID SendReport wrapper
  HID transmit state
  DataIn callback bridge
```

---

## 23. Bring-Up Milestones

```txt
M1: Generate USB_DEVICE HID project and build successfully.
M2: Wire target USB D-/D+/GND to PA11/PA12/GND and confirm enumeration.
M3: Convert CubeMX HID mouse/template descriptor into keyboard report descriptor.
M4: Send fixed report: A down + null report.
M5: Configure PB0-PB7 GPIO labels/modes for 4x4 matrix.
M6: Implement register-based ScanMatrix() using GPIOB->BSRR and GPIOB->IDR.
M7: Implement 5 ms timer scan request counter.
M8: Implement 4 scan buffers and normal debounce path.
M9: Implement CheckKeyOff -> CheckRepeat -> CheckKeyOn.
M10: Implement key ring buffer storing event + keyLoc.
M11: Implement key table and normal key conversion.
M12: Implement macro sequences Ctrl+C / Ctrl+V / Ctrl+S / Alt+Tab.
M13: Implement simultaneous-key ErrorRollOver report.
M14: Implement ErrorRecoveryCheck.
M15: Add README evidence logs, screenshots, and project page.
```

---

## 24. Final Design Summary

```txt
Input:
  4x4 matrix keypad without per-key diode

USB:
  USB_DEVICE HID class
  Target USB pins PA11/PA12 connected to external USB D-/D+
  Board powered from ST-LINK CN2 during bring-up

GPIO:
  Rows    PB0-PB3, output push-pull, default High
  Columns PB4-PB7, input pull-up

Scan:
  Timer period 5 ms
  Main scans only when scan request counter is non-zero
  GPIO access by register, not HAL
  Row settle delay by a few explicit __NOP() instructions
  The exact NOP count should be verified by oscilloscope or logic analyzer

Debounce:
  4 scan buffers
  Normal path uses 2 newest buffers
  Recovery path uses all 4 buffers

Detection order:
  CheckKeyOff()
  CheckRepeat()
  CheckKeyOn()

Error:
  If >= 2 keys are detected in the same raw scan:
    set simultaneous error
    enqueue ErrorRollOver event
    skip key off / repeat / key on for that scan

Recovery:
  If simultaneous condition remains, return
  Otherwise run recovery-specific key off / repeat / stable key check
  Normal key may be accepted after strict 4-buffer stability
  Macro key must be released and pressed again

Ring buffer:
  Stores key event + keyLoc only
  Does not store ASCII
  Does not store HID report bytes

Convert:
  Main convert pops key event
  Fetches key info from key table
  Starts normal / macro / error sequence
  Sends first report

USB DataIn callback:
  Does not pop new key event
  Sends next report of current sequence only
  Clears HID busy when sequence ends
```
