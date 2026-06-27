#!/usr/bin/env python3
#
# generate_metadata.py
#
# Generate Project 04 boot metadata binary.
#

import argparse
import struct
import sys
import zlib


BL_METADATA_MAGIC = 0x424C4D44      # "BLMD"
BL_METADATA_VERSION = 1

BL_METADATA_SLOT_A = 0x00000041     # 'A'
BL_METADATA_SLOT_B = 0x00000042     # 'B'


def slot_to_value(slot_name: str) -> int:
    slot_name = slot_name.upper()

    if slot_name == "A":
        return BL_METADATA_SLOT_A

    if slot_name == "B":
        return BL_METADATA_SLOT_B

    raise ValueError("slot must be A or B")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate Project 04 boot metadata binary"
    )

    parser.add_argument(
        "--active",
        required=True,
        choices=["A", "B", "a", "b"],
        help="active boot slot",
    )

    parser.add_argument(
        "--confirmed",
        required=True,
        choices=["A", "B", "a", "b"],
        help="confirmed boot slot",
    )

    parser.add_argument(
        "--boot-count",
        type=int,
        default=0,
        help="boot attempt count",
    )

    parser.add_argument(
        "-o",
        "--output",
        required=True,
        help="output metadata binary path",
    )

    args = parser.parse_args()

    active_slot = slot_to_value(args.active)
    confirmed_slot = slot_to_value(args.confirmed)

    reserved0 = 0
    reserved1 = 0
    reserved2 = 0

    payload = struct.pack(
        "<IIIIIIII",
        BL_METADATA_MAGIC,
        BL_METADATA_VERSION,
        active_slot,
        confirmed_slot,
        args.boot_count,
        reserved0,
        reserved1,
        reserved2,
    )

    crc32 = zlib.crc32(payload) & 0xFFFFFFFF

    metadata = payload + struct.pack("<I", crc32)

    with open(args.output, "wb") as f:
        f.write(metadata)

    print("[OK] Metadata generated")
    print(f"     output         = {args.output}")
    print(f"     magic          = 0x{BL_METADATA_MAGIC:08X}")
    print(f"     version        = {BL_METADATA_VERSION}")
    print(f"     active_slot    = {args.active.upper()}")
    print(f"     confirmed_slot = {args.confirmed.upper()}")
    print(f"     boot_count     = {args.boot_count}")
    print(f"     crc32          = 0x{crc32:08X}")
    print(f"     size           = {len(metadata)} bytes")

    return 0


if __name__ == "__main__":
    sys.exit(main())