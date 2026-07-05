"""
vendor_usb.py - USB vendor command layer for STM32 USB Composite Keypad+CDC+Bulk.

All four vendor commands use EP0 control IN (bmRequestType = 0xC0):
    GET_FIRMWARE_INFO  (0x01) - read firmware version, features, EP/IF map
    SET_REPEAT_ENABLE  (0x02) - toggle HID key-repeat in firmware
    SET_LED_MODE       (0x03) - control green LED (off/on/blink slow/fast)
    START_RAM_DUMP     (0x04) - trigger full SRAM1 stream over EP4 IN (Bulk)

Windows requirement: libusbK must be installed on Interface 3 (Vendor Data)
using Zadig. HID (IF0) and CDC (IF1/IF2) keep their inbox class drivers.

Public API:
    usb = VendorUsb()
    ok, product = usb.open()          # find device by VID:PID
    usb.close()
    info = usb.get_firmware_info()    # -> dict | None
    ok   = usb.set_repeat_enable(True/False)
    ok   = usb.set_led_mode(LED_ON)
    result = usb.start_ram_dump(progress_cb)  # -> (bytes, elapsed_s) | None
"""

import struct
import threading
import time

import usb.core
import usb.util
import usb.backend.libusb1

# Use bundled libusb1.dll from libusb-package if available else fall back to system
try:
    import libusb_package
    _BACKEND = libusb_package.get_libusb1_backend()
except ImportError:
    _BACKEND = usb.backend.libusb1.get_backend()

# ── Device identity ───────────────────────────────────────────────────────────
VID = 0x0483   # STMicroelectronics
PID = 0x572B   # STM32 USB Composite Keypad+CDC+Bulk

# ── EP0 vendor control IN ─────────────────────────────────────────────────────
VENDOR_GET = 0xC0   # bmRequestType: Device→Host | Vendor | Device

# bRequest codes - must match VENDOR_REQ_* in vendor_cmd.h
REQ_GET_FIRMWARE_INFO = 0x01
REQ_SET_REPEAT_ENABLE = 0x02
REQ_SET_LED_MODE      = 0x03
REQ_START_RAM_DUMP    = 0x04

# Response status - must match VENDOR_STATUS_* in vendor_cmd.h
VENDOR_STATUS_OK    = 0
VENDOR_STATUS_ERROR = 1

# ── Firmware feature flag bits (FirmwareInfo_t.featureFlags) ──────────────────
FW_FEATURE_HID_KEYBOARD  = 1 << 0
FW_FEATURE_CDC_LOG       = 1 << 1
FW_FEATURE_VENDOR_BULK   = 1 << 2
FW_FEATURE_REPEAT_CTRL   = 1 << 3

# Magic word in GET_FIRMWARE_INFO response: "P06C" little-endian
FW_INFO_MAGIC = 0x43363050

# ── LED mode values - must match VendorLedMode_t in vendor_cmd.h ──────────────
LED_OFF        = 0
LED_ON         = 1
LED_BLINK_SLOW = 2   # 500 ms toggle period
LED_BLINK_FAST = 3   # 125 ms toggle period

# ── Vendor bulk endpoint (Interface 3) ───────────────────────────────────────
VENDOR_IF_NUM = 3      # Interface index for Vendor Data
VENDOR_EP_IN  = 0x84   # EP4 IN, bulk, 64 bytes max packet size


class VendorUsb:
    """Thin wrapper around pyusb for the STM32 vendor interface."""

    def __init__(self):
        self._dev: usb.core.Device | None = None
        # Lock protects ctrl_transfer calls; dump runs in a worker thread
        self._lock = threading.Lock()

    @property
    def is_open(self) -> bool:
        return self._dev is not None

    def open(self) -> tuple[bool, str]:
        """Find the device by VID:PID and open it.

        Returns:
            (True, product_string) on success, (False, "") if not found.
        """
        dev = usb.core.find(idVendor=VID, idProduct=PID, backend=_BACKEND)
        if dev is None:
            return False, ""
        self._dev = dev
        try:
            product = dev.product or f"{VID:#06x}:{PID:#06x}"
        except Exception:
            # Some backends raise when reading string descriptors
            product = f"{VID:#06x}:{PID:#06x}"
        return True, product

    def close(self) -> None:
        """Release the device handle."""
        self._dev = None

    # ── Vendor commands ───────────────────────────────────────────────────────

    def get_firmware_info(self) -> dict | None:
        """Send GET_FIRMWARE_INFO and parse the 16-byte FirmwareInfo_t response.

        Layout: magic(4) vMajor(2) vMinor(2) features(4)
                hidIf(1) cdcCtrlIf(1) cdcDataIf(1) vendorIf(1)
                hidEp(1) cdcEp(1) bulkEp(1) reserved(1)

        Returns a dict with decoded fields, or None on failure/bad magic.
        """
        data = self._ctrl(REQ_GET_FIRMWARE_INFO, length=16)
        if data is None or len(data) < 16:
            return None

        magic, v_major, v_minor, features = struct.unpack_from("<IHHI", data, 0)
        if magic != FW_INFO_MAGIC:
            return None   # wrong device or corrupted response

        hid_if, cdc_ctrl_if, cdc_data_if, vendor_if = struct.unpack_from("BBBB", data, 8)
        hid_ep, cdc_ep, bulk_ep, _rsv              = struct.unpack_from("BBBB", data, 12)

        feature_names = []
        if features & FW_FEATURE_HID_KEYBOARD: feature_names.append("HID")
        if features & FW_FEATURE_CDC_LOG:      feature_names.append("CDC_LOG")
        if features & FW_FEATURE_VENDOR_BULK:  feature_names.append("VENDOR_BULK")
        if features & FW_FEATURE_REPEAT_CTRL:  feature_names.append("REPEAT_CTRL")

        return {
            "version":     f"{v_major}.{v_minor}",
            "features":    " | ".join(feature_names),
            "hid_if":      hid_if,
            "cdc_ctrl_if": cdc_ctrl_if,
            "cdc_data_if": cdc_data_if,
            "vendor_if":   vendor_if,
            "hid_ep":      f"0x{hid_ep:02X}",
            "cdc_ep":      f"0x{cdc_ep:02X}",
            "bulk_ep":     f"0x{bulk_ep:02X}",
        }

    def set_repeat_enable(self, enable: bool) -> bool:
        """Send SET_REPEAT_ENABLE. wValue=1 enables firmware key-repeat, 0 disables.

        Returns True if firmware echoes success status.
        """
        data = self._ctrl(REQ_SET_REPEAT_ENABLE, length=2, wValue=int(enable))
        return self._check_response(data, REQ_SET_REPEAT_ENABLE)

    def set_led_mode(self, mode: int) -> bool:
        """Send SET_LED_MODE. wValue = LED_OFF / LED_ON / LED_BLINK_SLOW / LED_BLINK_FAST.

        Returns True if firmware echoes success status.
        """
        data = self._ctrl(REQ_SET_LED_MODE, length=2, wValue=mode)
        return self._check_response(data, REQ_SET_LED_MODE)

    def start_ram_dump(self, progress_cb=None) -> tuple[bytes, float] | None:
        """Trigger a full SRAM1 dump (144 KB) and read it from EP4 IN.

        Step 1: EP0 GET_RAM_DUMP returns a 6-byte VendorDumpResponse_t
                {status(1), request(1), acceptedLength(4)}.
        Step 2: Read exactly acceptedLength bytes from the bulk endpoint.

        Args:
            progress_cb: Optional callable(received: int, total: int) called
                         after each chunk to report progress.

        Returns:
            (raw_bytes, elapsed_seconds) on success, None on failure.
        """
        # Step 1 - arm the dump via EP0 and read the accepted length
        data = self._ctrl(REQ_START_RAM_DUMP, length=6)
        if data is None or len(data) < 6:
            return None
        status, echoed, accepted_len = struct.unpack_from("<BBI", data)
        if status != VENDOR_STATUS_OK or echoed != REQ_START_RAM_DUMP:
            return None

        # Step 2 - read the bulk stream from EP4 IN
        received = bytearray()
        t_start = time.monotonic()

        try:
            usb.util.claim_interface(self._dev, VENDOR_IF_NUM)
        except usb.core.USBError:
            pass   # libusbK may not require explicit claim

        try:
            while len(received) < accepted_len:
                remaining = accepted_len - len(received)
                # Read up to one full FS bulk packet (64 bytes) at a time
                chunk = self._dev.read(VENDOR_EP_IN, min(remaining, 64), timeout=5000)
                received.extend(chunk)
                if progress_cb:
                    progress_cb(len(received), accepted_len)
        except usb.core.USBError:
            return None
        finally:
            try:
                usb.util.release_interface(self._dev, VENDOR_IF_NUM)
            except usb.core.USBError:
                pass

        elapsed = time.monotonic() - t_start
        return bytes(received), elapsed

    # ── Private helpers ───────────────────────────────────────────────────────

    def _ctrl(self, request: int, length: int,
              wValue: int = 0, wIndex: int = 0) -> bytes | None:
        """Issue a vendor control IN transfer and return the response bytes.

        Returns None if the device is not open or the transfer fails.
        """
        if self._dev is None:
            return None
        try:
            with self._lock:
                data = self._dev.ctrl_transfer(
                    VENDOR_GET, request,
                    wValue=wValue, wIndex=wIndex,
                    data_or_wLength=length)
            return bytes(data)
        except usb.core.USBError:
            return None

    @staticmethod
    def _check_response(data: bytes | None, expected_request: int) -> bool:
        """Validate a 2-byte VendorResponse_t {status, echoed_request}.

        Returns True only when status == OK and the echoed request matches.
        """
        if data is None or len(data) < 2:
            return False
        status, echoed = struct.unpack_from("BB", data)
        return status == VENDOR_STATUS_OK and echoed == expected_request
