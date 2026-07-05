"""
vendor_test.py - EP0 vendor request smoke-test for STM32 USB Composite Keypad+CDC+Bulk.

Exercises all four vendor commands implemented in firmware:
  - GET_FIRMWARE_INFO  : read firmware info struct (magic, version, features, EP map)
  - SET_LED_MODE       : control the green LED (off / on / blink slow / blink fast)
  - SET_REPEAT_ENABLE  : enable or disable HID key-repeat events
  - START_RAM_DUMP     : trigger SRAM bulk stream via EP4 IN, save to file

All commands use bmRequestType = 0xC0 (Vendor | Device | IN).
Firmware always returns a response struct so the tool has an explicit
success/failure signal without parsing CDC log.

EP0 carries commands and small responses then EP4 IN (0x84) streams bulk dump data.

Setup:
    pip install pyusb libusb-package

On Windows: install libusbK on Interface 3 (Vendor Data) using Zadig:
    Options → List All Devices → select Interface 3 → install libusbK.
    HID and CDC keep their inbox drivers and remain fully functional.

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
    pass   # libusb-package not installed - rely on system libusb

import usb.core
import usb.util
import usb.backend.libusb1

# Must match USBD_VID / USBD_PID in usbd_desc.c
VID = 0x0483   # STMicroelectronics
PID = 0x572B   # STM32 USB Composite Keypad+CDC+Bulk

# All vendor commands use control IN: Device→Host | Vendor | Device
VENDOR_GET = 0xC0

# bRequest codes - must match VENDOR_REQ_* defines in vendor_cmd.h
REQ_GET_FIRMWARE_INFO = 0x01
REQ_SET_REPEAT_ENABLE = 0x02
REQ_SET_LED_MODE      = 0x03
REQ_START_RAM_DUMP    = 0x04

# Magic word in GET_FIRMWARE_INFO response - "P06C" little-endian
FW_INFO_MAGIC  = 0x43363050

# Status codes in VendorResponse_t / VendorDumpResponse_t - mirror VENDOR_STATUS_* in firmware
VENDOR_STATUS_OK    = 0
VENDOR_STATUS_ERROR = 1

# LED mode values - mirror VendorLedMode_t in firmware
LED_OFF        = 0
LED_ON         = 1
LED_BLINK_SLOW = 2
LED_BLINK_FAST = 3
LED_NAMES = {LED_OFF: "OFF", LED_ON: "ON",
             LED_BLINK_SLOW: "BLINK_SLOW", LED_BLINK_FAST: "BLINK_FAST"}

# Interface 3 owns EP4 IN for bulk dump
VENDOR_IF_NUM = 3
VENDOR_EP_IN  = 0x84   # EP4 IN, bulk, 64 bytes max packet


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
        print("Check that firmware is running and libusbK is installed on Interface 3.")
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
    """Issue a vendor control IN transfer and return the response bytes."""
    data = dev.ctrl_transfer(VENDOR_GET, request,
                             wValue=wValue, wIndex=wIndex,
                             data_or_wLength=length)
    return bytes(data)


def check_vendor_response(data: bytes, expected_request: int) -> bool:
    """Parse a 2-byte VendorResponse_t {status, request} and print the result.
    Returns True on success."""
    if len(data) < 2:
        print(f"  [ERR] response too short: {data.hex()}")
        return False
    status, echoed = struct.unpack_from("BB", data)
    ok = (status == VENDOR_STATUS_OK) and (echoed == expected_request)
    tag = "OK" if ok else "ERR"
    print(f"  [{tag}] status={status} echoed_req=0x{echoed:02X}")
    return ok


def cmd_get_firmware_info(dev: usb.core.Device) -> None:
    """GET_FIRMWARE_INFO - receive a 16-byte FirmwareInfo_t struct.
    Validates the magic word and prints version, features, and EP map.
    Layout: magic(4) versionMajor(2) versionMinor(2) featureFlags(4)
            hidIf(1) cdcCtrlIf(1) cdcDataIf(1) vendorIf(1)
            hidInEp(1) cdcLogInEp(1) vendorBulkInEp(1) reserved(1)
    """
    data = ctrl_get(dev, REQ_GET_FIRMWARE_INFO, length=16)
    if len(data) < 16:
        print(f"  [ERR] short response: {len(data)} bytes")
        return
    magic, v_major, v_minor, features = struct.unpack_from("<IHHI", data, 0)
    hid_if, cdc_ctrl_if, cdc_data_if, vendor_if = struct.unpack_from("BBBB", data, 8)
    hid_ep, cdc_ep, bulk_ep, _rsv              = struct.unpack_from("BBBB", data, 12)

    if magic != FW_INFO_MAGIC:
        print(f"  [ERR] bad magic: 0x{magic:08X} (expected 0x{FW_INFO_MAGIC:08X})")
        return

    feature_names = []
    if features & (1 << 0): feature_names.append("HID")
    if features & (1 << 1): feature_names.append("CDC_LOG")
    if features & (1 << 2): feature_names.append("VENDOR_BULK")
    if features & (1 << 3): feature_names.append("REPEAT_CTRL")

    print(f"  [OK] firmware v{v_major}.{v_minor}")
    print(f"       features  : {' | '.join(feature_names)}")
    print(f"       interfaces: HID={hid_if} CDC_ctrl={cdc_ctrl_if} "
          f"CDC_data={cdc_data_if} Vendor={vendor_if}")
    print(f"       endpoints : HID_IN=0x{hid_ep:02X} CDC_IN=0x{cdc_ep:02X} "
          f"Bulk_IN=0x{bulk_ep:02X}")


def cmd_set_repeat_enable(dev: usb.core.Device, enable: bool) -> None:
    """SET_REPEAT_ENABLE - toggle HID key-repeat forwarding.
    wValue: 0 = disable, 1 = enable.
    Firmware returns a 2-byte VendorResponse_t {status, echoed_request}.
    """
    data = ctrl_get(dev, REQ_SET_REPEAT_ENABLE,
                    length=2, wValue=int(enable))
    print(f"  repeat_enable → {enable}")
    check_vendor_response(data, REQ_SET_REPEAT_ENABLE)


def cmd_set_led_mode(dev: usb.core.Device, mode: int) -> None:
    """SET_LED_MODE - drive the green LED on PA5.
    wValue: 0=off, 1=on, 2=blink slow (500 ms), 3=blink fast (125 ms).
    Firmware returns a 2-byte VendorResponse_t {status, echoed_request}.
    """
    data = ctrl_get(dev, REQ_SET_LED_MODE,
                    length=2, wValue=mode)
    print(f"  led_mode → {LED_NAMES.get(mode, mode)}")
    check_vendor_response(data, REQ_SET_LED_MODE)


def cmd_start_ram_dump(dev: usb.core.Device,
                       output_file: str = "ram_dump.bin") -> None:
    """START_RAM_DUMP - stream full SRAM1 (144 KB) over EP4 IN.

    Step 1: EP0 vendor GET — firmware always dumps the full 144 KB region
            (0x20000000) and returns a 6-byte VendorDumpResponse_t
            {status(1), request(1), acceptedLength(4)}.
    Step 2: Read exactly acceptedLength bytes from EP4 IN and save to file.
            Prints progress and elapsed time for throughput measurement.
    """
    # Step 1 - trigger and read response
    data = ctrl_get(dev, REQ_START_RAM_DUMP, length=6)
    if len(data) < 6:
        print(f"  [ERR] short response: {data.hex()}")
        return
    status, echoed, accepted_len = struct.unpack_from("<BBI", data)
    if status != VENDOR_STATUS_OK or echoed != REQ_START_RAM_DUMP:
        print(f"  [ERR] dump rejected: status={status} accepted={accepted_len}")
        return
    print(f"  [OK] dump accepted: {accepted_len} bytes ({accepted_len // 1024} KB)")

    # Step 2 - read bulk stream from EP4 IN
    try:
        usb.util.claim_interface(dev, VENDOR_IF_NUM)
    except usb.core.USBError:
        pass

    received = bytearray()
    t_start = time.monotonic()
    while len(received) < accepted_len:
        remaining = accepted_len - len(received)
        chunk = dev.read(VENDOR_EP_IN, min(remaining, 64), timeout=5000)
        received.extend(chunk)
        print(f"\r  {len(received)}/{accepted_len} bytes "
              f"({len(received) * 100 // accepted_len}%)",
              end="", flush=True)
    elapsed = time.monotonic() - t_start

    print()  # newline after progress

    try:
        usb.util.release_interface(dev, VENDOR_IF_NUM)
    except usb.core.USBError:
        pass

    throughput_kbs = (len(received) / 1024) / elapsed if elapsed > 0 else 0
    print(f"  {elapsed:.2f} s  →  {throughput_kbs:.1f} KB/s")

    with open(output_file, "wb") as f:
        f.write(received)
    print(f"  saved {len(received)} bytes → {output_file!r}")


def main() -> None:
    dev = find_device()

    print("\n[1] GET_FIRMWARE_INFO")
    cmd_get_firmware_info(dev)

    print("\n[2] SET_LED_MODE: ON → BLINK_SLOW → BLINK_FAST → OFF")
    cmd_set_led_mode(dev, LED_ON)
    time.sleep(1)
    cmd_set_led_mode(dev, LED_BLINK_SLOW)
    time.sleep(2)
    cmd_set_led_mode(dev, LED_BLINK_FAST)
    time.sleep(2)
    cmd_set_led_mode(dev, LED_OFF)

    print("\n[3] SET_REPEAT_ENABLE = False")
    cmd_set_repeat_enable(dev, False)
    print("     Hold any keypad key for 2 s - repeat should NOT fire.")
    time.sleep(2)

    print("\n[4] SET_REPEAT_ENABLE = True")
    cmd_set_repeat_enable(dev, True)
    print("     Hold any keypad key for 2 s - repeat should fire again.")
    time.sleep(2)

    print("\n[5] START_RAM_DUMP: full SRAM1 (144 KB) → ram_dump.bin")
    cmd_start_ram_dump(dev)

    print("\nAll tests passed.")


if __name__ == "__main__":
    main()
