"""
vendor_test.py — EP0 vendor request smoke-test for STM32 USB Composite Keypad+CDC.

Exercises all four vendor commands implemented in firmware:
  - GET_FIRMWARE_INFO  : read firmware version and name string
  - SET_LED_MODE       : control the green LED on the board (off / on / blink)
  - SET_REPEAT_ENABLE  : enable or disable HID key-repeat events
  - START_RAM_DUMP     : read SRAM region info (base address + size)

All commands use EP0 control transfers only — no bulk endpoint involved.

Setup:
    pip install pyusb libusb-package

On Windows: libusb-package bundles libusb1.dll so no manual driver install is
  needed for the first test.  If ctrl_transfer still fails with a pipe error,
  install the WinUSB driver with Zadig (https://zadig.akeo.ie): select the
  composite device (not a sub-interface) and install WinUSB or libusbK.

Usage:
    python vendor_test.py
"""

import struct
import sys
import time

try:
    import libusb_package
    libusb_package.get_libusb1_backend()   # register bundled libusb1.dll
except ImportError:
    pass   # libusb-package not installed — rely on system libusb

import usb.core
import usb.util
import usb.backend.libusb1

# Must match USBD_VID / USBD_PID in usbd_desc.c (1155 / 22315 decimal)
VID = 0x0483   # STMicroelectronics
PID = 0x572B   # STM32 USB Composite Keypad+CDC

# bmRequestType encodes direction | type | recipient.
# Both values use recipient = Device (0x00) because these are device-level
# vendor requests, not tied to a specific interface.
VENDOR_GET = 0xC0   # 1100 0000 — Device→Host | Vendor | Device
VENDOR_SET = 0x40   # 0100 0000 — Host→Device | Vendor | Device

# bRequest codes — must match the VENDOR_REQ_* defines in vendor_cmd.h
REQ_GET_FIRMWARE_INFO = 0x01
REQ_SET_REPEAT_ENABLE = 0x02
REQ_SET_LED_MODE      = 0x03
REQ_START_RAM_DUMP    = 0x04

# wValue constants for SET_LED_MODE — mirrors enum VendorLedMode_t in firmware
LED_OFF   = 0
LED_ON    = 1
LED_BLINK = 2
LED_NAMES = {LED_OFF: "OFF", LED_ON: "ON", LED_BLINK: "BLINK"}


def find_device() -> usb.core.Device:
    """Locate the device by VID/PID and return its handle; exit if not found."""
    try:
        import libusb_package
        backend = libusb_package.get_libusb1_backend()
    except ImportError:
        backend = usb.backend.libusb1.get_backend()
    dev = usb.core.find(idVendor=VID, idProduct=PID, backend=backend)
    if dev is None:
        print(f"Device {VID:#06x}:{PID:#06x} not found.")
        print("Make sure the firmware is running and the WinUSB/libusbK driver is installed.")
        sys.exit(1)
    try:
        mfr = dev.manufacturer or "<unknown>"
        prd = dev.product or "<unknown>"
    except Exception:
        mfr = prd = "<unavailable>"
    print(f"Found: {mfr!r} / {prd!r}  [{VID:#06x}:{PID:#06x}]")
    return dev


def ctrl_get(dev: usb.core.Device, request: int, length: int,
             wValue: int = 0, wIndex: int = 0) -> bytes:
    """Issue a vendor GET control transfer and return the response bytes."""
    data = dev.ctrl_transfer(VENDOR_GET, request,
                             wValue=wValue, wIndex=wIndex,
                             data_or_wLength=length)
    return bytes(data)


def ctrl_set(dev: usb.core.Device, request: int,
             wValue: int = 0, wIndex: int = 0) -> None:
    """Issue a vendor SET control transfer with no data phase (wLength = 0).
    The argument is carried in wValue, not in a data payload."""
    dev.ctrl_transfer(VENDOR_SET, request,
                      wValue=wValue, wIndex=wIndex,
                      data_or_wLength=0)


def cmd_get_firmware_info(dev: usb.core.Device) -> None:
    """GET_FIRMWARE_INFO — receive a 16-byte VendorFwInfo_t struct.
    Layout: [major, minor, patch, reserved(1), name[12]]
    """
    data = ctrl_get(dev, REQ_GET_FIRMWARE_INFO, length=16)
    major, minor, patch, _ = struct.unpack_from("BBBB", data)
    name = data[4:16].rstrip(b"\x00").decode("ascii", errors="replace")
    print(f"  firmware: v{major}.{minor}.{patch}  name={name!r}")


def cmd_set_repeat_enable(dev: usb.core.Device, enable: bool) -> None:
    """SET_REPEAT_ENABLE — tell the firmware whether to forward KEY_EVENT_REPEAT.
    When disabled, only the initial key-down fires; holding a key does nothing.
    """
    ctrl_set(dev, REQ_SET_REPEAT_ENABLE, wValue=int(enable))
    print(f"  repeat_enable → {enable}")


def cmd_set_led_mode(dev: usb.core.Device, mode: int) -> None:
    """SET_LED_MODE — change the state of the green LED on PA5.
    0 = off, 1 = on, 2 = blink at 250 ms cadence (driven by the main loop).
    """
    ctrl_set(dev, REQ_SET_LED_MODE, wValue=mode)
    print(f"  led_mode → {LED_NAMES.get(mode, mode)}")


def cmd_start_ram_dump(dev: usb.core.Device) -> None:
    """START_RAM_DUMP — receive an 8-byte VendorRamDumpInfo_t struct.
    Returns the base address and byte size of the SRAM region to be dumped.
    The actual bulk stream is not implemented yet; this only reads the metadata.
    """
    data = ctrl_get(dev, REQ_START_RAM_DUMP, length=8)
    # little-endian uint32 addr followed by uint32 size
    addr, size = struct.unpack_from("<II", data)
    print(f"  ram_dump: addr=0x{addr:08X}  size={size} bytes ({size // 1024} KB)")


def main() -> None:
    dev = find_device()

    print("\n[1] GET_FIRMWARE_INFO")
    cmd_get_firmware_info(dev)

    print("\n[2] SET_LED_MODE: ON for 1 s, then BLINK for 3 s, then OFF")
    cmd_set_led_mode(dev, LED_ON)
    time.sleep(1)
    cmd_set_led_mode(dev, LED_BLINK)
    time.sleep(3)
    cmd_set_led_mode(dev, LED_OFF)

    print("\n[3] SET_REPEAT_ENABLE = False")
    cmd_set_repeat_enable(dev, False)
    print("     Hold any keypad key for 2 s — repeat should NOT fire.")
    time.sleep(2)

    print("\n[4] SET_REPEAT_ENABLE = True")
    cmd_set_repeat_enable(dev, True)
    print("     Hold any keypad key for 2 s — repeat should fire again.")
    time.sleep(2)

    # TODO: verify bulk stream once START_RAM_DUMP full transfer is implemented
    print("\n[5] START_RAM_DUMP (info only, no bulk transfer yet)")
    cmd_start_ram_dump(dev)

    print("\nAll tests passed.")


if __name__ == "__main__":
    main()
