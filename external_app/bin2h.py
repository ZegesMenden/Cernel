#!/usr/bin/env python3
"""bin2h.py

Convert an arbitrary file into a C header that embeds the file as a byte array.

Example:
  python external_app/bin2h.py path/to/input.bin -o include/input_blob.h --name input_blob
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Iterable


_IDENTIFIER_RE = re.compile(r"[^0-9A-Za-z_]")


def _to_c_identifier(name: str) -> str:
    name = name.strip()
    if not name:
        return "data"
    name = _IDENTIFIER_RE.sub("_", name)
    if name[0].isdigit():
        name = "_" + name
    return name


def _iter_lines(data: bytes, bytes_per_line: int) -> Iterable[str]:
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i : i + bytes_per_line]
        yield ", ".join(f"0x{b:02x}" for b in chunk)


def _default_var_name(input_path: pathlib.Path) -> str:
    # Prefer stem, but handle files like ".bin".
    base = input_path.stem or input_path.name
    return _to_c_identifier(base)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Convert a binary file into a C header containing a byte array."
    )
    parser.add_argument("input", type=pathlib.Path, help="Input file to embed")
    parser.add_argument(
        "-o",
        "--output",
        type=pathlib.Path,
        default=None,
        help="Output header path (default: <input>.h next to input)",
    )
    parser.add_argument(
        "--name",
        default=None,
        help="C identifier for the byte array (default: derived from input name)",
    )
    parser.add_argument(
        "--bytes-per-line",
        type=int,
        default=12,
        help="How many bytes to emit per line (default: 12)",
    )
    parser.add_argument(
        "--no-pragma-once",
        action="store_true",
        help="Do not emit '#pragma once'",
    )
    parser.add_argument(
        "--no-static",
        action="store_true",
        help=(
            "Emit 'const' (external linkage) instead of 'static const'. "
            "Warning: including this header in multiple translation units will cause multiple-definition errors."
        ),
    )

    args = parser.parse_args(argv)

    input_path: pathlib.Path = args.input
    if not input_path.is_file():
        print(f"error: input is not a file: {input_path}", file=sys.stderr)
        return 2

    output_path: pathlib.Path
    if args.output is None:
        output_path = input_path.with_suffix(input_path.suffix + ".h")
    else:
        output_path = args.output

    var_name = _to_c_identifier(args.name) if args.name else _default_var_name(input_path)

    if args.bytes_per_line <= 0:
        print("error: --bytes-per-line must be > 0", file=sys.stderr)
        return 2

    data = input_path.read_bytes()
    linkage = "const" if args.no_static else "static const"

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", newline="\n", encoding="utf-8") as f:
        if not args.no_pragma_once:
            f.write("#pragma once\n")
        f.write("\n")
        f.write("#include <stddef.h>\n")
        f.write("#include <stdint.h>\n")
        f.write("\n")
        f.write(f"/* Embedded from: {input_path.as_posix()} ({len(data)} bytes) */\n")
        f.write(f"{linkage} uint8_t {var_name}[] = {{\n")
        for line in _iter_lines(data, args.bytes_per_line):
            f.write(f"    {line},\n")
        f.write("};\n")
        f.write(f"{linkage} size_t {var_name}_len = {len(data)};\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
