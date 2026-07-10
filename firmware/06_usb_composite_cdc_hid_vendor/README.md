# Project 06 — STM32G0 USB Composite: HID + CDC Log + Vendor Debug Tool

## 1. Project Goal

Project 06 extends the Project 05 USB HID macro keypad into a USB composite firmware
on STM32G0B1RE.

The composite device exposes four communication paths through a single USB cable:

```txt
HID Keyboard       : keypad input, macro keys, repeat, ErrorRollOver (reused from P05)
CDC ACM Log        : realtime firmware log stream to host terminal
EP0 Vendor Request : small command/control protocol from Python host tool
Vendor Bulk IN     : RAM dump stream to Python host tool
```

The HID keyboard engine — matrix scan, debounce, repeat, macro, simultaneous-key
policy — is inherited from Project 05 without changes. The focus of Project 06 is
composite USB architecture and host-side debug/control paths.

---

## 2. Project Status

```txt
Status        : Complete / Evidence captured
Base firmware : firmware/05_usb_hid_keyboard
Project path  : firmware/06_usb_composite_cdc_hid_vendor
```

Bring-up was done incrementally:

```txt
1. Copied Project 05 baseline; verified HID keyboard still enumerates.
2. Replaced HID-only class with composite class skeleton (4 interfaces, IAD, endpoint map).
3. Verified HID keyboard still sends key reports inside composite stack.
4. Added CDC ACM log interface and CdcLog_Printf ring buffer.
5. Added EP0 vendor control: GET_FIRMWARE_INFO, SET_REPEAT_ENABLE, SET_LED_MODE, START_RAM_DUMP.
6. Added Vendor Bulk IN RAM dump state machine (EP4, 64-byte chunks, DataIn callback).
7. Added Python GUI debug tool: CDC log panel + vendor command/dump panel.
8. Captured evidence: USBView, Zadig, Device Manager, Wireshark, TeraTerm, vendor_test.py, GUI.
```

---

## 3. Target Board

```txt
Board       : NUCLEO-G0B1RE
MCU         : STM32G0B1RE
IDE         : STM32CubeIDE / CubeMX
```

Same wiring as Project 05:

```txt
PA11 = USB_DM / D-
PA12 = USB_DP / D+
GND  = USB ground
```

Board is powered from ST-LINK CN2. Target USB breakout uses D-, D+, GND. VBUS is
not connected.

---

## 4. USB Architecture

### Interface Map

```txt
[IF0] HID Keyboard
      EP1 IN  interrupt  8 bytes  bInterval=0x0A (10 ms)

[IAD] bFirstInterface=1, bInterfaceCount=2  → groups IF1+IF2 as one CDC ACM function
[IF1] CDC Communication (ACM control)
      EP2 IN  interrupt  8 bytes  bInterval=0x10
[IF2] CDC Data
      EP3 OUT bulk  64 bytes
      EP3 IN  bulk  64 bytes

[IF3] Vendor Bulk Debug (bInterfaceClass=0xFF)
      EP4 IN  bulk  64 bytes
```

wTotalLength = 0x0074 (116 bytes).

### EP0 Vendor Commands

| bRequest | Name | wValue | Response |
|---|---|---|---|
| 0x01 | GET_FIRMWARE_INFO | - | `FirmwareInfo_t` (20 bytes) |
| 0x02 | SET_REPEAT_ENABLE | 0=off, 1=on | `VendorResponse_t` |
| 0x03 | SET_LED_MODE | 0=off, 1=on, 2=blink slow, 3=blink fast | `VendorResponse_t` |
| 0x04 | START_RAM_DUMP | - | `VendorDumpResponse_t` |

All commands use `bmRequestType = 0xC0` (control IN, device→host). Parameters are
carried in `wValue`; firmware always returns a response struct for explicit
success/failure signaling.

### Bulk IN Dump Flow

```txt
Host → START_RAM_DUMP via EP0
Firmware → VendorDumpResponse_t: status=OK, acceptedLength=147456
Host → reads 147456 bytes from EP4 IN
Firmware → streams 64-byte chunks via DataIn callback until done
```

Dump source: SRAM1 base `0x20000000`, fixed 144 KB. Measured throughput: ~421 KB/s
on USB Full Speed.

---

## 5. Module Structure

```txt
Core/Src/hid_keyboard_app.c     — reused from Project 05; main loop entry points
Core/Src/cdc_log.c              — ring buffer CDC log; CdcLog_Printf / CdcLog_Run
Core/Src/vendor_cmd.c           — EP0 vendor handler + bulk dump state machine

USB_Device/App/usbd_composite.c — composite class: descriptor, setup dispatch, DataIn
USB_Device/App/usbd_cdc_if.c    — CDC receive callback (not used for commands)
tools/vendor_test.py            — CLI test: 5 checks over 4 vendor commands
tools/composite_debug_tool/     — Python GUI: CDC log panel + vendor command panel
```

### CDC log safety

`CdcLog_Printf` must be called from the main loop only — it is not ISR-safe.

`VendorCmd_HandleSetup` runs in USB stack context and must not call `CdcLog_Printf`
directly. It sets a `VendorLogEvent_t` flag. `VendorCmd_FlushPendingLog()`, called
from `HID_Keyboard_App` in the main loop, reads the flag and does the actual log.

`VendorDump_Run()` handles the first-chunk kick and completion log from the main loop.
`VendorDump_OnTxCplt()` handles chunk-to-chunk streaming from the DataIn ISR, and
sets a `volatile bool gDumpDone` flag when the last chunk is sent — the main loop
reads this flag to log completion.

### Main loop call order

```c
void HID_Keyboard_App(void)
{
    /* drain scan ticks */
    while (ScanScheduler_TakeRequest() != 0U) { KeyDetect_Run(); }
    HidKeyboardConvert_Run();

    /* composite services */
    CdcLog_Run();
    VendorDump_Run(&hUsbDeviceFS);
    VendorCmd_FlushPendingLog();
    VendorCmd_UpdateLed();
}
```

---

## 6. Host Debug Tool

```txt
tools/vendor_test.py              — CLI test runner (no GUI dependency)
tools/composite_debug_tool/       — Python GUI tool
  main.py
  gui.py
  cdc_logger.py
  vendor_usb.py
  requirements.txt
```

`vendor_test.py` runs 5 checks over 4 vendor commands:
`GET_FIRMWARE_INFO`, `SET_REPEAT_ENABLE` on/off, `SET_LED_MODE`, `START_RAM_DUMP`.

Windows requires libusbK or WinUSB bound to IF3 via Zadig before the Python tool can
access the vendor interface. HID and CDC keep their inbox drivers — only IF3 needs
Zadig.

---

## 7. Evidence

```txt
assets/screenshots/06_usb_composite_cdc_hid_vendor/
  usbview-descriptor.png              — composite device, bDeviceClass=0xEF, 4 interfaces
  zadig-install-libusbk-interface3.png— libusbK bound to IF3 only
  device-manager-composite.png        — HID keyboard + CDC serial + vendor in one device
  wireshark-enumeration.png           — IAD visible in Configuration Descriptor
  vendor-test-and-cdc-log.png         — vendor_test.py output + TeraTerm CDC log
  composite-debug-tool-gui.png        — Python GUI with CDC log and vendor panels

assets/logs/06_usb_composite_cdc_hid_vendor/
  ep0_vendor_bulk_dump_test.txt       — vendor_test.py 5-test output
  cdc_log_session.txt                 — CDC log from TeraTerm session

assets/reports/06_usb_composite_cdc_hid_vendor/
  06_usb_composite_cdc_hid_vendor.pdf
```

### Evidence map

| Claim | Evidence |
|---|---|
| Composite device with 4 interfaces | usbview-descriptor.png |
| IAD groups IF1+IF2 as CDC ACM | wireshark-enumeration.png |
| HID, CDC, vendor in one Device Manager entry | device-manager-composite.png |
| Vendor IF3 needs Zadig / libusbK | zadig-install-libusbk-interface3.png |
| 4 vendor commands functional | ep0_vendor_bulk_dump_test.txt |
| CDC log visible alongside vendor commands | vendor-test-and-cdc-log.png |
| RAM dump throughput ~421 KB/s | ep0_vendor_bulk_dump_test.txt |
| Python GUI with CDC log + vendor panels | composite-debug-tool-gui.png |

---

## 8. Current Limitations

```txt
- Vendor interface on Windows requires Zadig/libusbK binding before the Python tool
  can use IF3. HID and CDC keep inbox drivers.
- RAM dump always covers full SRAM1 (144 KB fixed). Host cannot pass custom address/length.
- USBD_MAX_NUM_INTERFACES is set to 3 in usbd_conf.h (HID + CDC_Comm + CDC_Data).
  IF3 Vendor has no ST class driver entry; the macro is not incremented for it.
  This is intentional but should be noted if ST middleware is updated.
- Bus-powered standalone mode is not in scope.
- Persistent config in flash is not in scope.
- Microsoft OS 2.0 descriptors for automatic WinUSB binding are not implemented.
```

---

## 9. Future Improvements

```txt
- Microsoft OS 2.0 descriptors for automatic WinUSB binding (no Zadig needed).
- CDC OUT command shell.
- Flash page/range dump with configurable address and length.
- Persistent runtime config in flash.
- More polished Python GUI with device auto-detect.
- Bus-powered standalone hardware setup.
```
