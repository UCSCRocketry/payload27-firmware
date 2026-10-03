#!/usr/bin/env python3
#******************************************************************************
#
# SPDX-FileCopyrightText: Copyright (c) 2026, UCSC Rocket Team
#
# SPDX-License-Identifier: BSD-3-Clause
#
#******************************************************************************
"""
Load an image onto the Nucleo, or read its memory, with SEGGER J-Link Commander.

  jlink.py load [ELF]                    load an image and start it
  jlink.py read ADDRESS LENGTH OUTPUT    save target memory to a file

The J-Link device name, interface and speed come from jlink.ini next to this
script, or from the file given with --config.
"""

import argparse
import configparser
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

DEFAULT_CONFIG = Path(__file__).with_name("jlink.ini")
JLINK_EXE = "JLink.exe" if sys.platform == "win32" else "JLinkExe"


def load_config(path):
    """Read the [target] section of a jlink.ini."""
    config = configparser.ConfigParser()
    if not config.read(path):
        sys.exit(f"error: Cannot read {path}")
    try:
        target = config["target"]
        return {key: target[key] for key in ("device", "interface", "speed")}
    except KeyError:
        sys.exit(f"error: {path}: [target] needs device, interface and speed")


def jlink(target, *commands, timeout=180):
    """Run J-Link Commander commands against the target."""
    with tempfile.TemporaryDirectory() as tmp:
        script = Path(tmp) / "commands.jlink"
        # ExitOnError makes J-Link stop at the first failed command instead of
        # carrying on and reporting a confusing error much later.
        script.write_text("\n".join(["ExitOnError 1", *commands, "q"]) + "\n")
        try:
            result = subprocess.run(
                [JLINK_EXE, "-nogui", "1", "-if", target["interface"],
                 "-speed", target["speed"], "-device", target["device"],
                 "-CommanderScript", str(script)],
                capture_output=True, text=True, timeout=timeout,
            )
        except FileNotFoundError:
            sys.exit(f"error: {JLINK_EXE} not found on PATH; install the SEGGER J-Link tools")
        except subprocess.TimeoutExpired:
            sys.exit(f"error: {JLINK_EXE} did not finish within {timeout}s")

    if result.returncode != 0:
        sys.exit(f"error: {JLINK_EXE} failed:\n{result.stdout}\n{result.stderr}")


def elf_entry(elf):
    """Where to start an ELF image.

    Not e_entry itself: on Cortex-M that carries the Thumb bit in bit 0, and
    SetPC wants the instruction address.
    """
    with elf.open("rb") as f:
        header = f.read(0x1C)
    if len(header) < 0x1C or header[:6] != b"\x7fELF\x01\x01":
        sys.exit(f"error: {elf} is not a 32-bit little-endian ELF")
    return struct.unpack_from("<I", header, 0x18)[0] & ~1


def load(target, elf):
    """Put an image in target memory and start it at its entry point.

    The core is pointed at the entry rather than reset into the image, so this
    also works for images linked to run from RAM.
    """
    entry = elf_entry(elf)

    print(f"Loading {elf} (entry 0x{entry:08x})")
    jlink(target, "r", "h", f"loadfile \"{elf.resolve()}\"", f"SetPC 0x{entry:08x}", "g")


def read(target, address, length, output):
    """Save target memory to a file."""
    jlink(target, f"savebin \"{output.resolve()}\", 0x{address:08x}, 0x{length:x}")

    size = output.stat().st_size if output.is_file() else 0
    if size != length:
        sys.exit(f"error: Read {size} bytes from the target, expected {length}")


def number(text):
    return int(text, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                        help=f"J-Link settings (default: {DEFAULT_CONFIG.name} next to this script)")
    commands = parser.add_subparsers(dest="command", required=True)

    load_parser = commands.add_parser("load", help="load an ELF image and start it")
    load_parser.add_argument("elf", type=Path, nargs="?", default=Path("build/zephyr/zephyr.elf"),
                             help="image to load (default: build/zephyr/zephyr.elf)")

    read_parser = commands.add_parser("read", help="save target memory to a file")
    read_parser.add_argument("address", type=number, help="start address")
    read_parser.add_argument("length", type=number, help="number of bytes")
    read_parser.add_argument("output", type=Path, help="file to write")

    args = parser.parse_args()
    target = load_config(args.config)

    if args.command == "load":
        if not args.elf.is_file():
            sys.exit(f"error: {args.elf} not found; run west build first")
        load(target, args.elf)
    else:
        read(target, args.address, args.length, args.output)


if __name__ == "__main__":
    main()
