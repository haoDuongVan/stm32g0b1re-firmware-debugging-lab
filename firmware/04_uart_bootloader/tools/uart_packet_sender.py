#!/usr/bin/env python3
#
# uart_packet_sender.py
#
# Send binary packets to Project 04 bootloader update mode.
#
# Usage:
#   py -3 uart_packet_sender.py --port COM3 --file generated\test.bin --make-test-file --test-size 1024
#   py -3 uart_packet_sender.py --port COM3 --file app_slot_b.bin
#

import argparse
import os
import struct
import sys
import time
import zlib
from typing import Iterable, Tuple

import serial
from rich.console import Console
from rich.rule import Rule
from rich.table import Table
from rich.text import Text
from rich import box

console = Console(highlight=False)

PACKET_MAGIC        = 0x31544B50   # "PKT1"
PACKET_PAYLOAD_SIZE = 256          # must be multiple of 8

UPDATE_READY_TOKEN = b"[UPDATE] command_mode=READY"
UPDATE_PROMPT_TOKEN = b"UPDATE> "

TEST14_PASS = b"[TEST14] uart_update_mode_entry_check PASS"
TEST16_PASS = b"[TEST16] slot_erase_command_check PASS"
TEST16_FAIL = b"[TEST16] slot_erase_command_check FAIL"
TEST18_PASS = b"[TEST18] uart_binary_packet_receive_check PASS"
TEST18_FAIL = b"[TEST18] uart_binary_packet_receive_check FAIL"
APP_TOKEN   = b"[APP]"


# -----------------------------------------------------------------------------
# Console helpers  (rich for HOST lines; serial bytes go to sys.stdout.buffer)
# -----------------------------------------------------------------------------

def step(message: str) -> None:
    console.print(f"  [cyan]▶[/cyan] {message}")


def ok(message: str) -> None:
    console.print(f"  [bold green]✔[/bold green] {message}")


def warn(message: str) -> None:
    console.print(f"  [bold yellow]⚠[/bold yellow]  {message}")


def error(message: str) -> None:
    console.print(f"  [bold red]✘[/bold red] {message}")


def divider(title: str = "") -> None:
    console.print(Rule(title, style="dim"))


# -----------------------------------------------------------------------------
# File / packet helpers
# -----------------------------------------------------------------------------

def make_test_file(path: str, size: int) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)

    data = bytearray()

    while len(data) < size:
        line = f"PROJECT04-UART-PACKET-TEST-OFFSET-{len(data):08X}\n"
        data.extend(line.encode("ascii"))

    data = data[:size]

    with open(path, "wb") as f:
        f.write(data)

    ok(f"test file generated: {path} ({len(data)} bytes)")


def pad_to_8(payload: bytes) -> bytes:
    pad_len = (-len(payload)) % 8

    if pad_len == 0:
        return payload

    return payload + (b"\xFF" * pad_len)


def make_packet(offset: int, payload: bytes) -> bytes:
    crc32 = zlib.crc32(payload) & 0xFFFFFFFF
    header = struct.pack("<IIII", PACKET_MAGIC, offset, len(payload), crc32)
    return header + payload


# -----------------------------------------------------------------------------
# Serial session with persistent RX buffer
# -----------------------------------------------------------------------------

class SerialSession:
    def __init__(self, ser: serial.Serial, echo_serial: bool) -> None:
        self.ser = ser
        self.echo_serial = echo_serial
        self.rx_buffer = bytearray()

    def write(self, data: bytes) -> None:
        self.ser.write(data)
        self.ser.flush()

    def send_command(self, command: str, char_delay: float = 0.005) -> None:
        console.print(f"  [dim]CMD:[/dim] [bold]{command}[/bold]")
        # ReadLine echoes each byte with HAL_UART_Transmit (blocking ~87µs).
        # Sending at full UART speed (87µs/byte) causes STM32 RDR overrun.
        # 5ms per char is well above the echo time so no bytes are lost.
        for ch in command:
            self.ser.write(ch.encode("ascii"))
            self.ser.flush()
            time.sleep(char_delay)
        self.ser.write(b"\r")
        self.ser.flush()

    def wait_for(self, keyword: bytes, timeout_s: float) -> bytes:
        token, data = self.wait_for_any([keyword], timeout_s)
        return data

    def wait_for_any(self, keywords: Iterable[bytes], timeout_s: float) -> Tuple[bytes, bytes]:
        keyword_list = list(keywords)
        deadline = time.time() + timeout_s

        while time.time() < deadline:
            match = self._find_first_keyword(keyword_list)
            if match is not None:
                token, end_index = match
                data = bytes(self.rx_buffer[:end_index])
                del self.rx_buffer[:end_index]
                return token, data

            chunk = self.ser.read(256)

            if chunk:
                self.rx_buffer.extend(chunk)

                if self.echo_serial:
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()

        tail = bytes(self.rx_buffer[-512:]).decode("utf-8", errors="replace")
        expected = ", ".join(token.decode("ascii", errors="replace") for token in keyword_list)
        raise TimeoutError(f"timeout waiting for: {expected}\n--- rx tail ---\n{tail}")

    def _find_first_keyword(self, keywords: Iterable[bytes]):
        best_token = None
        best_start = None
        best_end = None

        for token in keywords:
            index = self.rx_buffer.find(token)

            if index >= 0:
                end_index = index + len(token)

                if best_start is None or index < best_start:
                    best_token = token
                    best_start = index
                    best_end = end_index

        if best_token is None:
            return None

        return best_token, best_end

    def wait_for_prompt(self, timeout_s: float = 10.0) -> None:
        self.wait_for(UPDATE_PROMPT_TOKEN, timeout_s)

    def sync_prompt(self) -> None:
        # Flush any stray 'u' left from the entry loop and wait for a fresh prompt.
        self.write(b"\r")
        self.wait_for_prompt(10.0)


# -----------------------------------------------------------------------------
# Phases
# -----------------------------------------------------------------------------

def enter_update_mode(sess: SerialSession) -> None:
    divider("enter update mode")
    step("reset the board now — sending 'u' for 10 s")

    deadline = time.time() + 10.0
    next_send = time.time()

    while time.time() < deadline:
        if time.time() >= next_send:
            sess.write(b"u")
            next_send = time.time() + 0.05

        try:
            sess.wait_for(UPDATE_READY_TOKEN, 0.10)
            ok("update mode entered")
            sess.sync_prompt()
            return
        except TimeoutError:
            pass

    raise TimeoutError("failed to enter update mode within 10 seconds")


def erase_slot_b(sess: SerialSession) -> None:
    divider("erase slot B")
    step("erasing Slot B  (192 KB)")

    sess.wait_for_prompt(10.0)
    sess.send_command("erase b")

    token, _ = sess.wait_for_any([TEST16_PASS, TEST16_FAIL], 60.0)

    if token == TEST16_PASS:
        ok("Slot B erased")
        return

    raise RuntimeError("erase Slot B failed: bootloader reported TEST16 FAIL")


def send_file_as_packets(sess: SerialSession, image_path: str) -> int:
    with open(image_path, "rb") as f:
        image = f.read()

    total_packets = (len(image) + PACKET_PAYLOAD_SIZE - 1) // PACKET_PAYLOAD_SIZE
    divider("send packets")
    step(f"sending {len(image)} bytes as {total_packets} packet(s)")

    offset = 0
    packet_index = 0

    while offset < len(image):
        chunk = image[offset:offset + PACKET_PAYLOAD_SIZE]
        payload = pad_to_8(chunk)
        packet = make_packet(offset, payload)

        console.print(f"  [dim]PKT {packet_index + 1}/{total_packets}[/dim]  offset=[cyan]0x{offset:08X}[/cyan]  size=[cyan]{len(payload)}[/cyan]")

        sess.wait_for_prompt(10.0)
        sess.send_command("rx-test b")
        sess.wait_for(b"[UPDATE] rx_packet_wait=START", 10.0)

        # Send header + payload in one write. The bootloader reads the payload
        # immediately after parsing the header, before printing long logs.
        sess.write(packet)

        token, _ = sess.wait_for_any([TEST18_PASS, TEST18_FAIL], 20.0)

        if token != TEST18_PASS:
            raise RuntimeError(f"packet {packet_index} failed: bootloader reported TEST18 FAIL")

        offset += PACKET_PAYLOAD_SIZE
        packet_index += 1

    ok(f"all {packet_index} packet(s) written and verified")
    return packet_index


def exit_update_mode(sess: SerialSession) -> None:
    divider("exit update mode")
    step("sending exit")

    sess.wait_for_prompt(10.0)
    sess.send_command("exit")

    try:
        sess.wait_for_any([APP_TOKEN, b"[BOOT] selected_slot="], 15.0)
        ok("bootloader continued normal boot")
    except TimeoutError:
        warn("exit sent, but no application/normal boot token was seen")


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Project 04 UART packet sender",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument("--port", required=True, help="COM port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument("--file", required=True, help="binary file to send")
    parser.add_argument("--make-test-file", action="store_true", help="generate ASCII test file before sending")
    parser.add_argument("--test-size", type=int, default=1024, help="test file size in bytes")
    parser.add_argument("--quiet-serial", action="store_true", help="do not print bootloader serial log")

    args = parser.parse_args()

    if args.make_test_file:
        make_test_file(args.file, args.test_size)

    if not os.path.exists(args.file):
        error(f"file not found: {args.file}")
        return 1

    file_size = os.path.getsize(args.file)
    total_packets = (file_size + PACKET_PAYLOAD_SIZE - 1) // PACKET_PAYLOAD_SIZE

    info = Table.grid(padding=(0, 2))
    info.add_column(style="bold cyan", min_width=8)
    info.add_column()
    info.add_row("port",    args.port)
    info.add_row("baud",    str(args.baud))
    info.add_row("file",    args.file)
    info.add_row("size",    f"{file_size} bytes")
    info.add_row("packets", str(total_packets))

    from rich.panel import Panel
    console.print()
    console.print(Panel(info, title="[bold]Project 04 — UART Packet Sender[/bold]", box=box.ROUNDED))
    console.print()

    packet_count = 0

    try:
        with serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.02,
            write_timeout=2.0,
        ) as ser:
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            sess = SerialSession(ser, echo_serial=(not args.quiet_serial))

            enter_update_mode(sess)
            erase_slot_b(sess)
            packet_count = send_file_as_packets(sess, args.file)
            exit_update_mode(sess)

    except (TimeoutError, RuntimeError) as exc:
        console.print()
        error(str(exc))
        return 1

    except serial.SerialException as exc:
        console.print()
        error(f"serial error: {exc}")
        return 1

    from rich.panel import Panel
    summary = Table.grid(padding=(0, 2))
    summary.add_column(style="bold", min_width=14)
    summary.add_column()
    summary.add_row("packets sent",  str(packet_count))
    summary.add_row("bytes written", str(packet_count * PACKET_PAYLOAD_SIZE))
    summary.add_row("slot",          "B")
    summary.add_row("result",        Text("PASS", style="bold green"))

    console.print()
    console.print(Panel(summary, title="[bold green]Update finished[/bold green]", box=box.ROUNDED))
    console.print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
