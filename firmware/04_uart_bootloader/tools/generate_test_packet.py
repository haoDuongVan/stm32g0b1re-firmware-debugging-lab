#!/usr/bin/env python3
#
# generate_test_packet.py
#
# Generate one UART binary test packet for Project 04 bootloader Milestone 18.
#
# Packet format (little-endian):
#   Offset  Size  Field
#   0       4     magic  = 0x31544B50  "PKT1"
#   4       4     offset  (byte offset into Slot B, must be 8-byte aligned)
#   8       4     length  (payload length in bytes, must be 8-byte aligned)
#   12      4     crc32   (CRC32/ISO-HDLC of payload only)
#   16      N     payload
#

import os
import struct
import zlib

PACKET_MAGIC = 0x31544B50  # "PKT1"
OFFSET = 0

# 64 bytes, multiple of 8 — satisfies STM32G0 doubleword alignment
PAYLOAD_TEXT = b"PKT18B00SLOTBWRITE-TEST-PAYLOAD-0001"
PAYLOAD = PAYLOAD_TEXT.ljust(64, b"\xA5")

crc32 = zlib.crc32(PAYLOAD) & 0xFFFFFFFF

header = struct.pack("<IIII", PACKET_MAGIC, OFFSET, len(PAYLOAD), crc32)
packet = header + PAYLOAD

script_dir = os.path.dirname(os.path.abspath(__file__))
out_dir = os.path.join(script_dir, "generated")
os.makedirs(out_dir, exist_ok=True)

out_path = os.path.join(out_dir, "uart_packet_test_slot_b.bin")

with open(out_path, "wb") as f:
    f.write(packet)

print(f"[OK] UART test packet generated")
print(f"     output  = {out_path}")
print(f"     magic   = 0x{PACKET_MAGIC:08X}")
print(f"     offset  = 0x{OFFSET:08X}")
print(f"     length  = {len(PAYLOAD)}")
print(f"     crc32   = 0x{crc32:08X}")
print(f"     payload = {PAYLOAD[:16].hex(' ')!r}...")
print()
print("After reset, Slot B initial_msp bytes (little-endian):")
print(f"     0x{struct.unpack_from('<I', PAYLOAD)[0]:08X}  ({PAYLOAD[:4]})")
