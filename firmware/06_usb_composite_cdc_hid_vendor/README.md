# Project 06 — STM32G0 USB Composite Keypad

## 1. Project Goal

Project 06 extends the Project 05 USB HID macro keypad into a USB composite firmware on STM32G0B1RE.

The target device exposes multiple communication paths through a single USB device:

```txt
HID Keyboard        : keypad input, macro keys, repeat, ErrorRollOver
CDC ACM Log         : realtime firmware log to PC
EP0 Vendor Request  : small command/control protocol from host tool
Vendor Bulk IN      : debug/RAM dump stream to host tool
Python GUI Tool     : CDC log viewer + vendor command/dump controller
```

The HID keyboard engine, matrix scan, debounce, repeat, macro, and simultaneous-key policy are inherited from Project 05. The main focus of Project 06 is USB composite architecture and host-side debug/control paths.

---

## 2. Project Status

```txt
Status        : In progress
Base firmware : Project 05 USB HID keyboard
Project path  : firmware/06_usb_composite_keypad
```

Implementation is developed incrementally:

```txt
M1: Start Project 06 from the Project 05 HID keyboard firmware
M2: Add USB composite descriptor skeleton
M3: Integrate HID keyboard into the composite stack
M4: Add CDC ACM log interface
M5: Add EP0 vendor request protocol
M6: Add Vendor Bulk IN RAM/debug dump
M7: Add Python GUI debug tool
M8: Add final evidence and documentation
```

---

## 3. Target Board

```txt
Board       : NUCLEO-G0B1RE
MCU         : STM32G0B1RE
IDE         : STM32CubeIDE / CubeMX
Base project: firmware/05_usb_hid_keyboard
New project : firmware/06_usb_composite_keypad
```

Important board notes:

```txt
ST-LINK CN2:
  Used for programming, debugging, and board power.
  Not the target USB HID/composite device path.

Target USB breakout:
  PA11 = USB_DM / D-
  PA12 = USB_DP / D+
  GND  = USB ground
```

During bring-up, the board is powered from ST-LINK CN2. The target USB breakout uses D-, D+, and GND. VBUS/5V bus-powered standalone mode is outside the main scope of this project.

---

## 4. Target USB Architecture

EP0 is the default control endpoint for the whole USB device. It is not a separate interface.

Target interfaces:

```txt
Interface 0: HID Keyboard
Interface 1: CDC Communication Interface
Interface 2: CDC Data Interface
Interface 3: Vendor Bulk Debug Interface
```

Target endpoint roles:

```txt
EP0:
  Standard USB requests
  HID class requests
  CDC class requests
  Vendor-specific requests

EP1 IN:
  HID Keyboard Interrupt IN

EP2 IN:
  CDC Notification Interrupt IN

EP3 IN:
  CDC Data Bulk IN

EP3 OUT:
  CDC Data Bulk OUT
  Present for standard CDC ACM behavior.

EP4 IN:
  Vendor Bulk IN
  Used to stream debug/RAM dump data to the host tool.
```

Endpoint numbers can be adjusted if required by the STM32 USB device stack. The interface roles and data paths remain the same.

---

## 5. Why Composite Device?

Project 05 proves that STM32G0B1RE can behave as a USB HID keyboard. Project 06 adds separate channels for firmware debugging and control.

```txt
HID:
  Main user-facing input path.

CDC ACM:
  Human-readable firmware log stream.

EP0 Vendor Request:
  Small command/response path for host tool control.

Vendor Bulk IN:
  Larger debug data path, such as RAM/debug buffer dump.
```

This separation keeps each channel focused. HID handles input, CDC handles logs, EP0 handles small control commands, and Bulk IN handles larger data transfer.

---

## 6. Reused HID Keypad Engine

The HID keypad behavior from Project 05 is preserved:

```txt
- 4x4 matrix keypad scan
- 2-of-2 debounce
- normal key event
- key repeat
- macro keys
- ErrorRollOver on simultaneous-key error
- tap-style HID output: key-down report followed by null report
```

The HID interface remains the user-facing input path. Additional CDC, vendor control, and bulk dump paths are added without changing the keypad concept.

---

## 7. CDC ACM Log Channel

CDC ACM is used as a realtime firmware log channel.

Typical log examples:

```txt
[BOOT] Project06 composite firmware started
[USB] configured
[CDC] line coding: 115200 8N1
[HID] keyLoc=4 usage=0x04
[HID] repeat enabled
[VREQ] GET_FIRMWARE_INFO SUCCESS
[VREQ] SET_REPEAT_ENABLE value=0 SUCCESS
[VREQ] START_RAM_DUMP len=147456 SUCCESS
[BULK] RAM dump started len=147456
[BULK] RAM dump complete len=147456
[ERR] simultaneous key detected
```

The CDC log stream is for observation and debugging. The host tool does not parse CDC logs to determine command success. Vendor command status is returned directly through EP0 responses.

---

## 8. EP0 Vendor Control Protocol

Request IDs:

```c
#define VREQ_GET_FIRMWARE_INFO   0x01U
#define VREQ_SET_REPEAT_ENABLE   0x02U
#define VREQ_START_RAM_DUMP      0x03U
#define VREQ_SET_LED_MODE        0x04U
```

Response status:

```c
#define VENDOR_RSP_SUCCESS       0x00U
#define VENDOR_RSP_FAILURE       0x01U
```

Common response:

```c
typedef struct __attribute__((packed))
{
    uint8_t status;   /* 0 = success, 1 = failure */
    uint8_t request;  /* echoed bRequest */
} VendorResponse_t;
```

Dump response:

```c
typedef struct __attribute__((packed))
{
    uint8_t  status;
    uint8_t  request;
    uint16_t acceptedLength;
} VendorDumpResponse_t;
```

Firmware info response:

```c
#define FW_INFO_MAGIC 0x43363050U /* "P06C" little-endian */

typedef struct __attribute__((packed))
{
    uint32_t magic;              /* offset  0: magic word, must equal FW_INFO_MAGIC */
    uint16_t versionMajor;       /* offset  4 */
    uint16_t versionMinor;       /* offset  6 */
    uint32_t featureFlags;       /* offset  8: bitmask, see FW_FEATURE_* defines */

    uint8_t  hidInterface;       /* offset 12 */
    uint8_t  cdcControlInterface;/* offset 13 */
    uint8_t  cdcDataInterface;   /* offset 14 */
    uint8_t  vendorInterface;    /* offset 15 */

    uint8_t  hidInEp;            /* offset 16 */
    uint8_t  cdcLogInEp;         /* offset 17 */
    uint8_t  vendorBulkInEp;     /* offset 18 */
    uint8_t  reserved;           /* offset 19 */
} FirmwareInfo_t; /* total: 20 bytes */
```

Command table:

| Command | bRequest | Direction | wValue | wIndex | EP0 response |
|---|---:|---|---:|---:|---|
| GET_FIRMWARE_INFO | 0x01 | IN `0xC0` | 0 | 0 | `FirmwareInfo_t` (20 byte) |
| SET_REPEAT_ENABLE | 0x02 | IN `0xC0` | 0=disable, 1=enable | 0 | `VendorResponse_t` |
| SET_LED_MODE | 0x03 | IN `0xC0` | 0=off, 1=on, 2=blink slow, 3=blink fast | 0 | `VendorResponse_t` |
| START_RAM_DUMP | 0x04 | IN `0xC0` | 0 | 0 | `VendorDumpResponse_t` |

SET-like commands use control IN because parameters fit in `wValue`/`wIndex` and the host tool needs a direct success/failure response from the device.

---

## 9. Vendor Bulk IN Dump Channel

EP0 starts a dump request. The actual dump payload is transferred through Vendor Bulk IN.

Dump flow:

```txt
1. Host sends START_RAM_DUMP via EP0 vendor request (no wValue/wIndex needed).
2. Firmware sets dump source to SRAM1 base (0x20000000) and size (144 KB).
3. EP0 returns VendorDumpResponse_t with acceptedLength = 147456.
4. Host reads exactly acceptedLength bytes from Vendor Bulk IN (EP4).
5. DataIn callback drives the stream — 64 bytes per packet until done.
```

The dump always covers the full SRAM1 region (144 KB). The host does not pass
an arbitrary address or length — firmware sets both to avoid reading invalid
memory. Throughput measured at ~421 KB/s on USB Full Speed.

---

## 10. Firmware Structure

The firmware keeps the HID keypad engine reusable while adding composite-specific modules.

```txt
hid_keyboard_app.c/.h
  Reused and adapted from Project 05.

usb_composite_core.c/.h
  Composite descriptor, setup dispatch, endpoint dispatch.

cdc_log.c/.h
  Firmware log output through CDC ACM data IN.

vendor_request.c/.h
  EP0 vendor-specific request handling.

vendor_bulk_dump.c/.h
  Debug buffer and Bulk IN dump state machine.

led_control.c/.h
  LED off/on/blink modes controlled by vendor request.

app_config.c/.h
  Runtime config such as repeat enable flag.
```

`main.c` stays thin:

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USB_DEVICE_Init();
    MX_TIM6_Init();

    App_Init();

    while (1)
    {
        App_Run();
    }
}
```

---

## 11. Host Debug Tool

Host tool folder:

```txt
tools/composite_debug_tool/
  main.py
  gui.py
  cdc_logger.py
  vendor_usb.py
  hex_view.py
  requirements.txt
  README.md
```

Dependencies:

```txt
Python 3.x
tkinter
pyserial
pyusb
libusb backend / WinUSB binding for vendor interface
```

GUI layout:

```txt
Left:
  CDC COM port selection
  realtime CDC log viewer
  clear/save CDC log buttons

Right:
  Get Firmware Info
  Enable/Disable Repeat
  LED ON/OFF/Blink
  RAM dump offset/length
  response/dump output viewer
```

Thread model:

```txt
Main thread:
  tkinter GUI

CDC reader thread:
  serial.readline()
  queue.put(log line)

Vendor dump worker:
  send START_RAM_DUMP
  read Vendor Bulk IN
  queue.put(hex dump output)
```

The GUI updates text widgets through queue polling from the main thread.

---

## 12. Build and Bring-Up

Initial setup:

```txt
1. Copy firmware/05_usb_hid_keyboard to firmware/06_usb_composite_keypad.
2. Remove Debug/Release build output folders from the copied project.
3. Rename the CubeIDE project metadata.
4. Rename the IOC file to match the new project folder.
5. Import firmware/06_usb_composite_keypad into STM32CubeIDE.
6. Build once without functional changes.
7. Flash and confirm HID keyboard behavior before adding composite changes.
```

PowerShell helper commands:

```powershell
Copy-Item -Recurse firmware\05_usb_hid_keyboard firmware\06_usb_composite_keypad
Remove-Item -Recurse -Force firmware\06_usb_composite_keypad\Debug, firmware\06_usb_composite_keypad\Release -ErrorAction SilentlyContinue
```

CubeIDE project name:

```xml
<name>06_usb_composite_keypad</name>
```

Import path:

```txt
STM32CubeIDE
  File
  Import...
  General
  Existing Projects into Workspace
  Select root directory: firmware/06_usb_composite_keypad
```

---

## 13. Bring-Up Milestones

### M1 — Start Project 06 from Project 05

```txt
- Project folder exists as firmware/06_usb_composite_keypad.
- CubeIDE project name is updated.
- IOC file is renamed.
- Firmware builds successfully.
- HID keyboard still enumerates and sends key reports.
```

### M2 — Add Composite Descriptor Skeleton

```txt
- HID interface
- CDC Communication interface
- CDC Data interface
- Vendor debug interface
- IAD for CDC ACM
- Endpoint descriptors
```

Evidence:

```txt
USBView shows multiple interfaces in one USB device.
```

### M3 — Integrate HID Keyboard into Composite Stack

```txt
- HID endpoint works inside composite device.
- Keypad sends normal key reports.
- Macro, repeat, and ErrorRollOver still work.
```

### M4 — Add CDC ACM Log Interface

```txt
- CDC ACM descriptors and class requests.
- CDC data IN log output.
- CDC OUT endpoint exists for standard CDC ACM behavior.
- Log boot, USB configured, key event, vendor command, dump start/complete.
```

### M5 — Add EP0 Vendor Request Protocol

```txt
- GET_FIRMWARE_INFO
- SET_REPEAT_ENABLE
- SET_LED_MODE
- START_RAM_DUMP response only
```

### M6 — Add Vendor Bulk IN RAM Dump

```txt
- Debug dump buffer.
- START_RAM_DUMP prepares dump state.
- Bulk IN streams acceptedLength bytes.
- Host script reads dump data.
```

### M7 — Add Python GUI Tool

```txt
- CDC log panel.
- Vendor command buttons.
- RAM dump output.
- Save log/output.
```

### M8 — Add Final Evidence and Documentation

```txt
- USBView screenshots.
- Device Manager screenshots.
- CDC log screenshot.
- Python GUI screenshots.
- Wireshark USBPcap captures.
- README update.
```

---

## 14. Evidence Checklist

### USBView

```txt
- Device Descriptor
- Configuration Descriptor
- HID interface
- CDC Communication interface
- CDC Data interface
- Vendor-specific interface
- Endpoint list
```

### Device Manager

```txt
- USB Composite Device
- HID Keyboard Device
- USB Serial Device (COMx)
- Vendor interface driver binding if WinUSB/libusb is used
```

### Wireshark / USBPcap

```txt
- GET_DESCRIPTOR Configuration
- SET_CONFIGURATION
- HID interrupt IN report
- CDC SET_LINE_CODING
- CDC data IN log transfer
- EP0 vendor request GET_FIRMWARE_INFO
- EP0 vendor request SET_REPEAT_ENABLE
- EP0 vendor request SET_LED_MODE
- EP0 vendor request START_RAM_DUMP
- Vendor Bulk IN dump data
```

### Python GUI

```txt
- CDC log connected and receiving firmware log.
- Get Firmware Info response displayed.
- Enable/Disable Repeat response displayed.
- LED ON/OFF/Blink response displayed.
- RAM dump hex output displayed.
```

---

## 15. Current Limitations

```txt
- CDC OUT endpoint exists for standard CDC ACM behavior, but command control uses EP0 vendor requests.
- Vendor interface on Windows requires libusbK/WinUSB binding via Zadig before the Python tool can use IF3.
- RAM dump always covers the full SRAM1 region (144 KB fixed). The host cannot pass a custom address or length.
- USBD_MAX_NUM_INTERFACES is set to 3U in usbd_conf.h (HID+CDC_Comm+CDC_Data). The Vendor interface (IF3)
  has no ST class driver entry, so the macro is not incremented for it. This is intentional but should be
  noted if the ST middleware is updated.
- Bus-powered standalone mode is not in the main scope.
- Persistent config in flash is not in the main scope.
- FreeRTOS and UART DMA logger are not in the main scope.
```

---

## 16. Future Improvements

```txt
- Microsoft OS 2.0 descriptors for automatic WinUSB binding.
- CDC OUT command shell.
- Flash page/range dump.
- Persistent runtime config in flash.
- UART DMA logger.
- FreeRTOS task-based architecture.
- More polished Python GUI with device auto-detect.
- Bus-powered standalone hardware setup.
```
