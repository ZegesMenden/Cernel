"""Concatenate input files into a single C file.

Usage:
  python concat.py <file1> [file2 ...]

Writes: concat.c (in the current working directory)
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path


_CHUNK_SIZE = 64 * 1024


def _marker_bytes(kind: str, display_path: str) -> bytes:
	# Keep markers ASCII-friendly and very obvious in the output.
	return f"\n/* ===== {kind} FILE: {display_path} ===== */\n".encode("utf-8")


def concat_files(input_paths: list[Path], output_path: Path) -> None:
	output_path = output_path.resolve()

	with output_path.open("wb") as out:
		out.write(b"/* concat.py output: concatenated sources */\n")
		out.write(b"/* DO NOT EDIT: re-run concat.py instead. */\n")

		for input_path in input_paths:
			display_path = str(input_path)
			resolved_input = input_path.resolve()

			out.write(_marker_bytes("BEGIN", display_path))

			last_byte: int | None = None
			with resolved_input.open("rb") as src:
				while True:
					chunk = src.read(_CHUNK_SIZE)
					if not chunk:
						break
					out.write(chunk)
					last_byte = chunk[-1]

			# Ensure the END marker starts on a new line.
			if last_byte not in (None, 0x0A):
				out.write(b"\n")

			out.write(_marker_bytes("END", display_path))


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description=(
			"Incrementally reads each input file and concatenates them into concat.c, "
			"with C-style BEGIN/END markers around each file."
		)
	)
	parser.add_argument(
		"paths",
		nargs="+",
		help="Input file paths to concatenate (in the order given)",
	)
	return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
	args = _parse_args(argv)

	input_paths = [Path(p) for p in args.paths]
	missing = [p for p in input_paths if not p.is_file()]
	if missing:
		for p in missing:
			print(f"error: not a file: {p}")
		return 2

	try:
		concat_files(input_paths, Path(os.getcwd()) / "concat.c")
	except OSError as exc:
		print(f"error: {exc}")
		return 1

	print(f"Wrote {len(input_paths)} file(s) to concat.c")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
