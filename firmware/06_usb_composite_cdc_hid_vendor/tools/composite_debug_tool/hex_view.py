"""
hex_view.py - Format raw bytes as a human-readable hex dump string.

Each row shows: offset, hex bytes (16 per row), printable ASCII.
Non-printable bytes are shown as '.' in the ASCII column.

Example output (16 bytes per row):
    0000: 50 30 36 43 00 00 06 00 0F 00 00 00 00 01 02 03  P06C............
    0010: 81 83 84 00                                      ....
"""

# ASCII printable range: 0x20 (' ') to 0x7E ('~')
_PRINTABLE = set(range(0x20, 0x7F))


def format_hex_dump(data: bytes, bytes_per_row: int = 16) -> str:
    """Return a multi-line hex dump string for the given bytes.

    Args:
        data:         Raw bytes to format.
        bytes_per_row: Number of bytes displayed per row (default 16).

    Returns:
        Formatted string; "(empty)" if data is empty.
    """
    if not data:
        return "(empty)"

    lines = []
    for offset in range(0, len(data), bytes_per_row):
        chunk = data[offset: offset + bytes_per_row]

        # Hex column: space-separated two-digit uppercase hex values
        hex_part = " ".join(f"{b:02X}" for b in chunk)
        # Pad to fixed width so the ASCII column always starts at the same position
        hex_part = hex_part.ljust(bytes_per_row * 3 - 1)

        # ASCII column: printable chars as-is, others as '.'
        ascii_part = "".join(chr(b) if b in _PRINTABLE else "." for b in chunk)
        lines.append(f"{offset:04X}: {hex_part}  {ascii_part}")

    return "\n".join(lines)
