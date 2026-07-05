"""
cdc_logger.py - Background serial reader for the STM32 CDC log interface.

The firmware sends UTF-8 log lines over a CDC ACM virtual COM port.
This module opens the port in a daemon thread, reads lines, and pushes
them into a queue that the GUI polls on the main thread.

Usage:
    logger = CdcLogger(queue)
    logger.connect("COM5", 115200)   # starts reader thread
    # GUI polls queue with after()
    logger.disconnect()              # stops thread and closes port
"""

import queue
import serial
import threading


class CdcLogger:
    def __init__(self, log_queue: queue.Queue):
        # Shared queue; GUI consumes from the main thread via after()-poll
        self._queue = log_queue
        self._port: serial.Serial | None = None
        self._thread: threading.Thread | None = None
        # Event used to signal the reader thread to stop cleanly
        self._stop_event = threading.Event()

    @property
    def is_connected(self) -> bool:
        return self._port is not None and self._port.is_open

    def connect(self, port: str, baudrate: int = 115200) -> None:
        """Open the COM port and start the background reader thread."""
        if self.is_connected:
            self.disconnect()  # clean up any previous connection first

        self._stop_event.clear()
        # timeout=0.1 s lets the reader loop check the stop event regularly
        self._port = serial.Serial(port, baudrate, timeout=0.1)
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()
        self._queue.put(f"[CDC] connected on {port} @ {baudrate}\n")

    def disconnect(self) -> None:
        """Signal the reader thread to stop, then close the port."""
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        if self._port is not None:
            try:
                self._port.close()
            except Exception:
                pass
            self._port = None
        self._queue.put("[CDC] disconnected\n")

    def _reader(self) -> None:
        """Reader loop: runs in daemon thread, pushes decoded lines to queue."""
        assert self._port is not None
        while not self._stop_event.is_set():
            try:
                line = self._port.readline()
                if line:
                    # replace=True: don't crash on garbled bytes during firmware init
                    self._queue.put(line.decode("utf-8", errors="replace"))
            except serial.SerialException as exc:
                # Port unplugged or lost - report and exit loop
                self._queue.put(f"[CDC][ERR] {exc}\n")
                break
