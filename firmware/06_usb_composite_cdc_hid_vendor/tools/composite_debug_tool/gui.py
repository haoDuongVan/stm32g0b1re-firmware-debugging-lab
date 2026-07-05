"""
gui.py - tkinter GUI for STM32 USB Composite debug tool.

Layout (two columns):
    Left  (40%): CDC LOG panel - COM port selector, realtime firmware log
    Right (60%): VENDOR CONTROL panel - USB commands, RAM dump, hex output

Thread model:
    Main thread    : tkinter event loop; only thread allowed to update widgets
    CDC reader     : serial.readline() → log_queue  (daemon, in cdc_logger.py)
    Dump worker    : bulk read → result_queue        (daemon, spawned on demand)

The main thread polls both queues every POLL_MS ms via after().
"""

import queue
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

import serial.tools.list_ports

from cdc_logger import CdcLogger
from hex_view import format_hex_dump
from vendor_usb import LED_BLINK_FAST, LED_BLINK_SLOW, LED_OFF, LED_ON, VendorUsb

# -- Help text shown in the Help dialog ---------------------------------------
_HELP_TEXT = """\
STM32 USB Composite Debug Tool - Quick Start

--- CDC Log (left panel) --------------------------------
1. Plug in the STM32 board.
2. Select the correct COM port from the dropdown.
   (Tip: use Refresh if the port does not appear.)
3. Click Connect - firmware log lines appear in real time.
4. Use Clear to wipe the log; Save… to export it as .txt.

--- Vendor Control (right panel) ------------------------
1. Install libusbK on Interface 3 via Zadig:
      Options → List All Devices
      Select "STM32 USB Composite … (Interface 3)"
      Driver: libusbK → Install
   HID and CDC keep their inbox drivers and stay functional.
2. Click Connect USB - the product name appears when found.
3. Get Firmware Info - reads version, features, EP map.
4. Enable / Disable Repeat - controls HID firmware repeat.
   Use the Test field: hold a key and count characters.
5. LED buttons - drive the green LED on the board.
6. Start Dump - streams full SRAM1 (144 KB) over EP4 IN.
   Progress bar shows transfer; throughput is printed on done.
   Save Dump… exports the full binary (.bin) or hex (.txt).

--- Notes -----------------------------------------------
• CDC log and USB vendor are independent - both can be
  connected at the same time.
• Vendor buttons are disabled until USB is connected.
• Save Dump… is disabled until a dump completes.
"""


# -- Tooltip helper ------------------------------------------------------------

class _Tooltip:
    """Show a small popup label when the mouse hovers over a widget."""

    _DELAY_MS = 600    # delay before the tooltip appears
    _PAD = 4           # padding inside the tooltip box

    def __init__(self, widget: tk.Widget, text: str):
        self._widget = widget
        self._text = text
        self._job: str | None = None   # after() job id
        self._tip: tk.Toplevel | None = None

        widget.bind("<Enter>", self._on_enter, add="+")
        widget.bind("<Leave>", self._on_leave, add="+")
        widget.bind("<ButtonPress>", self._on_leave, add="+")

    def _on_enter(self, _event=None):
        self._job = self._widget.after(self._DELAY_MS, self._show)

    def _on_leave(self, _event=None):
        if self._job:
            self._widget.after_cancel(self._job)
            self._job = None
        self._hide()

    def _show(self):
        if self._tip:
            return
        x = self._widget.winfo_rootx() + self._widget.winfo_width() // 2
        y = self._widget.winfo_rooty() + self._widget.winfo_height() + 4

        self._tip = tk.Toplevel(self._widget)
        self._tip.wm_overrideredirect(True)   # no window chrome
        self._tip.wm_geometry(f"+{x}+{y}")

        lbl = tk.Label(
            self._tip, text=self._text, justify="left",
            background="#ffffe0", relief="solid", borderwidth=1,
            font=("Segoe UI", 8), padx=self._PAD, pady=self._PAD)
        lbl.pack()

    def _hide(self):
        if self._tip:
            self._tip.destroy()
            self._tip = None


def tooltip(widget: tk.Widget, text: str) -> _Tooltip:
    """Attach a hover tooltip to widget and return the Tooltip object."""
    return _Tooltip(widget, text)


# -- Main application window ---------------------------------------------------

class App(tk.Tk):
    POLL_MS = 50   # queue-poll interval in milliseconds

    def __init__(self):
        super().__init__()
        self.title("STM32 USB Composite Debug Tool")
        self.resizable(True, True)


        # Queues are the only safe way to pass data from daemon threads to the GUI
        self._log_queue: queue.Queue[str] = queue.Queue()
        self._result_queue: queue.Queue = queue.Queue()

        self._logger = CdcLogger(self._log_queue)
        self._usb = VendorUsb()
        self._last_dump_bytes: bytes | None = None   # kept for Save Dump
        self._out_has_content = False                # controls first-entry separator
        self._vendor_btns: list[ttk.Button] = []     # all buttons gated on USB connect

        self._build_ui()
        self._refresh_ports()
        self.geometry("1000x800")
        self.minsize(900, 700)
        # Start the poll loop after the event loop is running
        self.after(self.POLL_MS, self._poll)

    # -- UI construction -------------------------------------------------------

    def _build_ui(self):
        # CDC panel is narrower - vendor panel needs more width for hex output
        # uniform="col" forces exact 4:6 split - without it tkinter only distributes
        # leftover space after minimum widget sizes, so ratio drifts toward 6:4.
        self.columnconfigure(0, weight=4, uniform="col")   # CDC log
        self.columnconfigure(1, weight=6, uniform="col")   # Vendor control
        self.rowconfigure(0, weight=1)

        self._build_cdc_panel(column=0)
        self._build_vendor_panel(column=1)

    def _build_cdc_panel(self, column: int):
        frame = ttk.LabelFrame(self, text="CDC LOG", padding=6)
        frame.grid(row=0, column=column, sticky="nsew", padx=(8, 4), pady=8)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(2, weight=1)   # log text expands vertically

        # -- COM port row --
        conn_row = ttk.Frame(frame)
        conn_row.grid(row=0, column=0, sticky="ew", pady=(0, 4))
        conn_row.columnconfigure(1, weight=1)

        ttk.Label(conn_row, text="Port:").grid(row=0, column=0, padx=(0, 4))
        self._port_var = tk.StringVar()
        self._port_cb = ttk.Combobox(conn_row, textvariable=self._port_var, width=10)
        self._port_cb.grid(row=0, column=1, sticky="ew", padx=(0, 4))
        tooltip(self._port_cb, "Select the CDC virtual COM port.\nUse Refresh if it does not appear.")

        refresh_btn = ttk.Button(conn_row, text="Refresh", command=self._refresh_ports, takefocus=False)
        refresh_btn.grid(row=0, column=2, padx=(0, 4))
        tooltip(refresh_btn, "Re-scan available COM ports.")

        self._connect_btn = ttk.Button(conn_row, text="Connect", command=self._toggle_cdc, takefocus=False)
        self._connect_btn.grid(row=0, column=3)
        tooltip(self._connect_btn,
                "Open the selected COM port at 115200 baud\n"
                "and start streaming the firmware CDC log.")

        # -- Connection status label --
        self._cdc_status_var = tk.StringVar(value="Disconnected")
        ttk.Label(frame, textvariable=self._cdc_status_var,
                  foreground="gray").grid(row=1, column=0, sticky="w")

        # -- Scrollable log text (read-only) --
        self._log_text = scrolledtext.ScrolledText(
            frame, state="disabled", wrap="word",
            font=("Consolas", 9), height=36)
        self._log_text.grid(row=2, column=0, sticky="nsew")

        # -- Bottom buttons --
        btn_row = ttk.Frame(frame)
        btn_row.grid(row=3, column=0, sticky="e", pady=(4, 0))
        ttk.Button(btn_row, text="Clear", command=self._clear_log,
                   takefocus=False).pack(side="left", padx=2)
        ttk.Button(btn_row, text="Save…", command=self._save_log,
                   takefocus=False).pack(side="left", padx=2)

    def _build_vendor_panel(self, column: int):
        frame = ttk.LabelFrame(self, text="VENDOR CONTROL / BULK DUMP", padding=6)
        frame.grid(row=0, column=column, sticky="nsew", padx=(4, 8), pady=8)
        frame.columnconfigure(0, weight=1)

        row = 0

        # -- USB device connect row --
        usb_row = ttk.Frame(frame)
        usb_row.grid(row=row, column=0, sticky="ew", pady=(0, 4))
        self._usb_status_var = tk.StringVar(value="Device not connected")
        ttk.Label(usb_row, textvariable=self._usb_status_var,
                  foreground="gray").pack(side="left")

        # Help button - opens usage dialog
        help_btn = ttk.Button(usb_row, text="?", width=2, command=self._show_help,
                              takefocus=False)
        help_btn.pack(side="right", padx=(16, 0))
        tooltip(help_btn, "Show quick-start help.")

        self._usb_btn = ttk.Button(usb_row, text="Connect USB", command=self._toggle_usb,
                                   takefocus=False)
        self._usb_btn.pack(side="right")
        tooltip(self._usb_btn,
                "Find the STM32 composite device (VID 0483 PID 572B).\n"
                "Requires libusbK installed on Interface 3 via Zadig.\n"
                "HID and CDC keep their inbox drivers.")
        row += 1

        ttk.Separator(frame, orient="horizontal").grid(
            row=row, column=0, sticky="ew", pady=4)
        row += 1

        # -- Helpers to create vendor buttons (disabled until USB is connected) --
        def _vbtn(parent, text, command):
            b = ttk.Button(parent, text=text, command=command,
                           state="disabled", takefocus=False)
            self._vendor_btns.append(b)
            return b

        def _vbtn_pack(parent, text, command):
            b = _vbtn(parent, text, command)
            b.pack(side="left", padx=2)
            return b

        # -- Device Info --
        info_lf = ttk.LabelFrame(frame, text="Device Info", padding=4)
        info_lf.grid(row=row, column=0, sticky="ew", pady=(0, 6))
        info_lf.columnconfigure(0, weight=1)
        info_btn = _vbtn(info_lf, "Get Firmware Info", self._cmd_get_info)
        info_btn.grid(row=0, column=0, sticky="w")
        tooltip(info_btn,
                "EP0 GET_FIRMWARE_INFO (0x01)\n"
                "Returns version, feature flags, interface numbers, endpoint addresses.")
        row += 1

        # -- Key Repeat --
        repeat_lf = ttk.LabelFrame(frame, text="Key Repeat", padding=4)
        repeat_lf.grid(row=row, column=0, sticky="ew", pady=(0, 6))
        repeat_lf.columnconfigure(0, weight=1)

        btn_row = ttk.Frame(repeat_lf)
        btn_row.grid(row=0, column=0, sticky="w")
        en_btn = _vbtn_pack(btn_row, "Enable Repeat",  lambda: self._cmd_set_repeat(True))
        dis_btn = _vbtn_pack(btn_row, "Disable Repeat", lambda: self._cmd_set_repeat(False))
        tooltip(en_btn,  "EP0 SET_REPEAT_ENABLE wValue=1\nFirmware will forward KEY_REPEAT events to HID.")
        tooltip(dis_btn, "EP0 SET_REPEAT_ENABLE wValue=0\nFirmware will drop KEY_REPEAT events.")

        self._repeat_state_var = tk.StringVar(value="State: Enabled")
        self._repeat_state_lbl = ttk.Label(btn_row, textvariable=self._repeat_state_var, foreground="green")
        self._repeat_state_lbl.pack(side="left", padx=(12, 0))

        # Test field - receives HID keyboard input directly
        test_row = ttk.Frame(repeat_lf)
        test_row.grid(row=1, column=0, sticky="ew", pady=(6, 0))
        test_row.columnconfigure(1, weight=1)
        ttk.Label(test_row, text="Test (hold a key):").grid(row=0, column=0, padx=(0, 6))
        self._repeat_test_var = tk.StringVar()
        repeat_entry = ttk.Entry(test_row, textvariable=self._repeat_test_var)
        repeat_entry.grid(row=0, column=1, sticky="ew")
        tooltip(repeat_entry,
                "Click here, then hold a keypad key.\n"
                "With repeat enabled: characters accumulate.\n"
                "With repeat disabled: only one character appears.")
        clr_btn = ttk.Button(test_row, text="Clear",
                             command=lambda: self._repeat_test_var.set(""),
                             takefocus=False)
        clr_btn.grid(row=0, column=2, padx=(4, 0))
        tooltip(clr_btn, "Clear the repeat test field.")

        ttk.Label(repeat_lf,
                  text="Hold a keypad key - count chars to verify repeat on/off.",
                  foreground="gray").grid(row=2, column=0, sticky="w", pady=(2, 0))
        row += 1

        # -- LED Control --
        led_lf = ttk.LabelFrame(frame, text="LED Control", padding=4)
        led_lf.grid(row=row, column=0, sticky="ew", pady=(0, 6))
        led_tips = {
            LED_OFF:        "EP0 SET_LED_MODE wValue=0\nTurn LED off immediately.",
            LED_ON:         "EP0 SET_LED_MODE wValue=1\nTurn LED on immediately.",
            LED_BLINK_SLOW: "EP0 SET_LED_MODE wValue=2\nBlink every 500 ms.",
            LED_BLINK_FAST: "EP0 SET_LED_MODE wValue=3\nBlink every 125 ms.",
        }
        for text, mode in [("OFF", LED_OFF), ("ON", LED_ON),
                            ("Blink Slow", LED_BLINK_SLOW), ("Blink Fast", LED_BLINK_FAST)]:
            b = _vbtn_pack(led_lf, text, lambda m=mode: self._cmd_set_led(m))
            tooltip(b, led_tips[mode])
        row += 1

        # -- RAM Dump --
        dump_lf = ttk.LabelFrame(frame, text="RAM Dump  (full SRAM1, 144 KB)", padding=4)
        dump_lf.grid(row=row, column=0, sticky="ew", pady=(0, 6))
        dump_lf.columnconfigure(0, weight=1)

        self._dump_btn = _vbtn(dump_lf, "Start Dump", self._cmd_start_dump)
        self._dump_btn.grid(row=0, column=0, sticky="w", pady=(0, 4))
        tooltip(self._dump_btn,
                "EP0 START_RAM_DUMP (0x04)\n"
                "Arms the firmware bulk pipeline, then reads 144 KB\n"
                "from EP4 IN (0x84) at full FS bulk speed.")

        # Progress bar and throughput label
        self._dump_progress = ttk.Progressbar(dump_lf, mode="determinate")
        self._dump_progress.grid(row=1, column=0, sticky="ew")
        self._dump_progress_lbl = tk.StringVar(value="")
        ttk.Label(dump_lf, textvariable=self._dump_progress_lbl).grid(
            row=2, column=0, sticky="w")
        row += 1

        # -- Response / Dump Output --
        # This row gets all remaining vertical space
        frame.rowconfigure(row, weight=1)
        out_lf = ttk.LabelFrame(frame, text="Response / Dump Output", padding=4)
        out_lf.grid(row=row, column=0, sticky="nsew")
        out_lf.columnconfigure(0, weight=1)
        out_lf.rowconfigure(0, weight=1)

        self._out_text = scrolledtext.ScrolledText(
            out_lf, state="disabled", wrap="none",
            font=("Consolas", 9), height=14)
        self._out_text.grid(row=0, column=0, sticky="nsew")

        btn_row = ttk.Frame(out_lf)
        btn_row.grid(row=1, column=0, sticky="e", pady=(4, 0))

        clr_out = ttk.Button(btn_row, text="Clear", command=self._clear_output, takefocus=False)
        clr_out.pack(side="left", padx=2)
        tooltip(clr_out, "Clear the output panel.")

        save_log = ttk.Button(btn_row, text="Save Log…", command=self._save_output, takefocus=False)
        save_log.pack(side="left", padx=2)
        tooltip(save_log, "Save the text currently displayed\nin the output panel as .txt.")

        self._save_dump_btn = ttk.Button(btn_row, text="Save Dump…",
                                         command=self._save_dump, state="disabled",
                                         takefocus=False)
        self._save_dump_btn.pack(side="left", padx=2)
        tooltip(self._save_dump_btn,
                "Save the full binary from the last successful dump.\n"
                ".bin → raw bytes   .txt → full hex dump\n"
                "Enabled only after a successful Start Dump.")

    # -- CDC actions -----------------------------------------------------------

    def _refresh_ports(self):
        """Re-scan serial ports and populate the COM port combobox."""
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self._port_cb["values"] = ports
        if ports and not self._port_var.get():
            self._port_var.set(ports[0])

    def _toggle_cdc(self):
        """Connect to or disconnect from the CDC COM port."""
        if self._logger.is_connected:
            self._logger.disconnect()
            self._connect_btn.config(text="Connect")
            self._cdc_status_var.set("Disconnected")
        else:
            port = self._port_var.get()
            if not port:
                messagebox.showwarning("No port", "Select a COM port first.")
                return
            try:
                self._logger.connect(port, 115200)
                self._connect_btn.config(text="Disconnect")
                self._cdc_status_var.set(f"Connected  {port} @ 115200")
            except Exception as exc:
                messagebox.showerror("Connect failed", str(exc))

    def _clear_log(self):
        self._log_text.config(state="normal")
        self._log_text.delete("1.0", "end")
        self._log_text.config(state="disabled")

    def _save_log(self):
        """Save the CDC log text to a file."""
        path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self._log_text.get("1.0", "end"))

    # -- USB vendor actions ----------------------------------------------------

    def _set_vendor_btns_state(self, state: str):
        """Enable or disable all vendor command buttons at once."""
        for btn in self._vendor_btns:
            btn.config(state=state)

    def _toggle_usb(self):
        """Open or close the USB vendor device handle."""
        if self._usb.is_open:
            self._usb.close()
            self._usb_btn.config(text="Connect USB")
            self._usb_status_var.set("Device not connected")
            self._set_vendor_btns_state("disabled")
            self._save_dump_btn.config(state="disabled")
        else:
            ok, product = self._usb.open()
            if ok:
                self._usb_btn.config(text="Disconnect USB")
                self._usb_status_var.set(f"Connected: {product}")
                self._set_vendor_btns_state("normal")
            else:
                messagebox.showerror(
                    "USB not found",
                    f"Device 0x0483:0x572B not found.\n\n"
                    "Steps to fix:\n"
                    "1. Check the board is plugged in.\n"
                    "2. Open Zadig → Options → List All Devices.\n"
                    "3. Select Interface 3, install libusbK driver.")

    def _show_help(self):
        """Show the quick-start help dialog."""
        win = tk.Toplevel(self)
        win.title("Help - Quick Start")
        win.resizable(False, False)
        txt = scrolledtext.ScrolledText(win, wrap="word", font=("Consolas", 9),
                                        width=62, height=34, state="normal")
        txt.insert("1.0", _HELP_TEXT)
        txt.config(state="disabled")
        txt.pack(padx=8, pady=8)
        ttk.Button(win, text="Close", command=win.destroy).pack(pady=(0, 8))

    def _cmd_get_info(self):
        """Send GET_FIRMWARE_INFO and display the decoded response."""
        info = self._usb.get_firmware_info()
        if info is None:
            self._out_section("[ERR] GET_FIRMWARE_INFO failed\n")
            return
        lines = [
            "[OK] GET_FIRMWARE_INFO",
            f"  Version    : {info['version']}",
            f"  Features   : {info['features']}",
            f"  Interfaces : HID={info['hid_if']}  CDC_ctrl={info['cdc_ctrl_if']}"
            f"  CDC_data={info['cdc_data_if']}  Vendor={info['vendor_if']}",
            f"  Endpoints  : HID_IN={info['hid_ep']}  CDC_IN={info['cdc_ep']}"
            f"  Bulk_IN={info['bulk_ep']}",
            "",
        ]
        self._out_section("\n".join(lines))

    def _cmd_set_repeat(self, enable: bool):
        """Send SET_REPEAT_ENABLE and display success/failure."""
        ok = self._usb.set_repeat_enable(enable)
        state = "enabled" if enable else "disabled"
        tag = "OK" if ok else "ERR"
        self._out_section(f"[{tag}] SET_REPEAT_ENABLE → {state}\n")
        if ok:
            self._repeat_state_var.set(f"State: {'Enabled' if enable else 'Disabled'}")
            self._repeat_state_lbl.config(foreground="green" if enable else "gray")

    def _cmd_set_led(self, mode: int):
        """Send SET_LED_MODE and display success/failure."""
        names = {LED_OFF: "OFF", LED_ON: "ON",
                 LED_BLINK_SLOW: "BLINK_SLOW", LED_BLINK_FAST: "BLINK_FAST"}
        ok = self._usb.set_led_mode(mode)
        tag = "OK" if ok else "ERR"
        self._out_section(f"[{tag}] SET_LED_MODE → {names.get(mode, mode)}\n")

    def _cmd_start_dump(self):
        """Trigger START_RAM_DUMP and launch the bulk read in a worker thread."""
        self._dump_btn.config(state="disabled")
        self._save_dump_btn.config(state="disabled")
        self._dump_progress["value"] = 0
        self._dump_progress_lbl.set("Starting…")
        self._out_section("[...] START_RAM_DUMP - reading 144 KB from EP4 IN…\n")

        def progress_cb(received: int, total: int):
            # Called from the worker thread - communicate via queue
            self._result_queue.put(("progress", received, total))

        def worker():
            result = self._usb.start_ram_dump(progress_cb=progress_cb)
            self._result_queue.put(("dump_done", result))

        threading.Thread(target=worker, daemon=True).start()

    # -- Output panel helpers --------------------------------------------------

    def _out_append(self, text: str):
        """Append text to the output panel (must be called from main thread)."""
        self._out_text.config(state="normal")
        self._out_text.insert("end", text)
        self._out_text.see("end")
        self._out_text.config(state="disabled")

    def _out_section(self, text: str):
        """Append text with a separator line before it (skipped on first entry)."""
        if self._out_has_content:
            self._out_append("─" * 60 + "\n")
        self._out_has_content = True
        self._out_append(text)

    def _clear_output(self):
        self._out_text.config(state="normal")
        self._out_text.delete("1.0", "end")
        self._out_text.config(state="disabled")
        self._out_has_content = False   # next entry won't get a leading separator

    def _save_output(self):
        """Save the text currently displayed in the output panel."""
        path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self._out_text.get("1.0", "end"))

    def _save_dump(self):
        """Save the full binary from the last successful START_RAM_DUMP.

        .bin → raw bytes written directly.
        .txt → complete hex dump (all bytes, not just the 512-byte preview).
        """
        if self._last_dump_bytes is None:
            return
        path = filedialog.asksaveasfilename(
            defaultextension=".bin",
            filetypes=[("Binary dump", "*.bin"), ("Hex text", "*.txt"), ("All files", "*.*")])
        if not path:
            return
        if path.endswith(".txt"):
            with open(path, "w", encoding="utf-8") as f:
                f.write(format_hex_dump(self._last_dump_bytes))
        else:
            with open(path, "wb") as f:
                f.write(self._last_dump_bytes)

    # -- Main-thread queue poll ------------------------------------------------

    def _poll(self):
        """Drain both queues and update widgets; rescheduled every POLL_MS ms."""

        # CDC log lines from the reader thread
        try:
            while True:
                line = self._log_queue.get_nowait()
                self._log_text.config(state="normal")
                self._log_text.insert("end", line)
                self._log_text.see("end")
                self._log_text.config(state="disabled")
        except queue.Empty:
            pass

        # Results from the dump worker thread
        try:
            while True:
                item = self._result_queue.get_nowait()
                if item[0] == "progress":
                    _, received, total = item
                    pct = received * 100 // total
                    self._dump_progress["value"] = pct
                    self._dump_progress_lbl.set(f"{received}/{total} bytes  ({pct}%)")
                elif item[0] == "dump_done":
                    self._on_dump_done(item[1])
        except queue.Empty:
            pass

        self.after(self.POLL_MS, self._poll)

    def _on_dump_done(self, result):
        """Handle dump completion - update progress, display preview, enable Save."""
        self._dump_btn.config(state="normal")

        if result is None:
            self._out_append("[ERR] RAM dump failed\n")
            self._dump_progress_lbl.set("Failed")
            return

        data, elapsed = result
        kbs = (len(data) / 1024) / elapsed if elapsed > 0 else 0
        self._dump_progress["value"] = 100
        self._dump_progress_lbl.set(
            f"{len(data)} bytes  |  {elapsed:.2f} s  |  {kbs:.1f} KB/s")

        # Store full binary for Save Dump and enable the button
        self._last_dump_bytes = data
        self._save_dump_btn.config(state="normal")

        # Show only a 512-byte preview in the output panel
        preview = min(512, len(data))
        self._out_append(
            f"[OK] RAM dump complete: {len(data)} bytes  "
            f"({elapsed:.2f} s  {kbs:.1f} KB/s)\n"
            f"[Preview: first {preview} of {len(data)} bytes]\n\n")
        self._out_append(format_hex_dump(data[:preview]))
        if len(data) > preview:
            self._out_append(
                f"\n… ({len(data) - preview} more bytes hidden"
                f" - use Save Dump… to export the full binary)\n")
        self._out_append("\n")
