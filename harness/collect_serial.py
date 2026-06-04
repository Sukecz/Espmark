#!/usr/bin/env python3
"""Collect one espmark JSON result bundle from a serial port."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import serial


BEGIN = "ESPMARK_RESULT_BEGIN"
END = "ESPMARK_RESULT_END"


def collect(port: str, baud: int, timeout: float) -> dict:
    lines: list[str] = []
    in_bundle = False

    with serial.Serial(port, baudrate=baud, timeout=timeout) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if line == BEGIN:
                lines.clear()
                in_bundle = True
                continue
            if line == END and in_bundle:
                return json.loads("\n".join(lines))
            if in_bundle:
                lines.append(line)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    bundle = collect(args.port, args.baud, args.timeout)
    text = json.dumps(bundle, indent=2, sort_keys=True) + "\n"

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()

