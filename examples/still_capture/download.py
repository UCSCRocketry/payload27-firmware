#!/usr/bin/env python3
#******************************************************************************
#
# SPDX-FileCopyrightText: Copyright (c) 2026, UCSC Rocket Team
#
# SPDX-License-Identifier: BSD-3-Clause
#
#******************************************************************************
"""
Copy the still_capture picture out of target RAM over SWD and save it.
"""

import argparse
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from collections import namedtuple
from pathlib import Path

# struct still_capture_result in src/main.c: six little-endian uint32 fields.
Result = namedtuple("Result", "magic width height length crc32 address")
RESULT_LAYOUT = struct.Struct("<6I")
RESULT_MAGIC = 0x4349505A  # "ZPIC"

JLINK = Path(__file__).resolve().parents[2] / "scripts" / "jlink.py"


def symbol_address(map_file, name):
    """Look a symbol up in zephyr.map. Addresses move on every build."""
    for line in map_file.read_text(errors="replace").splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[1] == name and fields[0].startswith("0x"):
            return int(fields[0], 16)
    sys.exit(f"error: {map_file}: no symbol {name}")


def jlink(*args):
    """Run scripts/jlink.py, which holds the J-Link settings for the board."""
    sys.stdout.flush()
    if subprocess.run([sys.executable, str(JLINK), *args]).returncode != 0:
        sys.exit(1)


def read_memory(address, length):
    """Read target RAM over SWD."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "memory.bin"
        jlink("read", f"0x{address:08x}", f"0x{length:x}", str(path))
        return path.read_bytes()


def wait_for_picture(args, address):
    """Poll the descriptor until the sample says a picture is ready."""
    deadline = time.monotonic() + args.timeout

    while True:
        result = Result._make(RESULT_LAYOUT.unpack(read_memory(address, RESULT_LAYOUT.size)))
        if result.magic == RESULT_MAGIC:
            return result

        if time.monotonic() > deadline:
            sys.exit(
                f"error: No picture after {args.timeout}s (magic is 0x{result.magic:08x}, "
                f"expected 0x{RESULT_MAGIC:08x}). Check the RTT log"
            )

        time.sleep(0.5)


def read_picture(result):
    """Copy the picture bytes out of target RAM and check them."""
    if not 0 < result.length <= 32 * 1024 * 1024:
        sys.exit(f"error: Implausible picture length {result.length}; is the descriptor stale?")

    data = read_memory(result.address, result.length)

    crc = zlib.crc32(data)
    if crc != result.crc32:
        sys.exit(
            f"error: Checksum mismatch: target says {result.crc32:08x}, got {crc:08x}. "
            "The picture was read while it was still changing, or the cache was "
            "not flushed."
        )

    return data


def save(data, result, output):
    output.write_bytes(data)
    print(f"Saved {len(data)} bytes: JPEG {result.width}x{result.height} -> {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-d", "--build-dir", type=Path, default=Path("build"),
                        help="west build directory (default: build)")
    parser.add_argument("-o", "--output", type=Path, default=Path("picture.jpg"),
                        help="where to save the picture (default: picture.jpg)")
    parser.add_argument("--timeout", type=float, default=30.0,
                        help="seconds to wait for a picture (default: 30)")
    args = parser.parse_args()

    map_file = args.build_dir / "zephyr" / "zephyr.map"
    if not map_file.is_file():
        sys.exit(f"error: {args.build_dir} does not look built; run west build first")

    result_addr = symbol_address(map_file, "still_capture_result")

    print(f"Waiting for a picture (descriptor at 0x{result_addr:08x})")
    result = wait_for_picture(args, result_addr)
    save(read_picture(result), result, args.output)


if __name__ == "__main__":
    main()
