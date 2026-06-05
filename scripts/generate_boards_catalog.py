#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import urllib.request
from pathlib import Path


SOC_LABELS = {
    "esp8266": "ESP8266",
    "esp32": "ESP32",
    "esp32c3": "ESP32-C3",
    "esp32c5": "ESP32-C5",
    "esp32c6": "ESP32-C6",
    "esp32p4": "ESP32-P4",
    "esp32s2": "ESP32-S2",
    "esp32s3": "ESP32-S3",
}

PLATFORMIO_BOARDS_API = "https://api.github.com/repos/platformio/platform-espressif32/contents/boards?ref=develop"
PLATFORMIO_ESP8266_BOARDS_API = "https://api.github.com/repos/platformio/platform-espressif8266/contents/boards?ref=develop"
VENDOR_PREFIXES = [
    "adafruit",
    "arduino",
    "dfrobot",
    "espressif",
    "lilygo",
    "m5stack",
    "olimex",
    "seeed studio",
    "seeed",
    "sparkfun",
    "waveshare",
]


def read_json_url(url: str):
    request = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def parse_arduino_boards_txt(path: Path, source: str) -> list[dict[str, str]]:
    boards: dict[str, dict[str, str]] = {}
    if not path.exists():
        return []

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        parts = key.split(".")
        if len(parts) < 2:
            continue

        board_id = parts[0]
        prop = ".".join(parts[1:])
        board = boards.setdefault(board_id, {"id": board_id})

        if prop == "name":
            board["name"] = value.strip()
        elif prop == "build.mcu":
            board["mcu"] = value.strip().lower()
        elif prop == "build.variant":
            board["variant"] = value.strip()
        elif prop == "build.board":
            board["arduino_board"] = value.strip()

    catalog_boards = []
    for board in boards.values():
        mcu = board.get("mcu", "")
        name = board.get("name", "")
        if mcu not in SOC_LABELS or not name:
            continue
        catalog_boards.append({
            "id": f"{source}:{board['id']}",
            "source": source,
            "source_id": board["id"],
            "name": name,
            "soc": SOC_LABELS[mcu],
            "mcu": mcu,
            "vendor": "",
            "variant": board.get("variant", ""),
            "url": "",
        })
    return catalog_boards


def parse_platformio_boards() -> list[dict[str, str]]:
    return parse_platformio_board_api(PLATFORMIO_BOARDS_API, "platformio-espressif32")


def parse_platformio_esp8266_boards() -> list[dict[str, str]]:
    return parse_platformio_board_api(PLATFORMIO_ESP8266_BOARDS_API, "platformio-espressif8266")


def parse_platformio_board_api(api_url: str, source: str) -> list[dict[str, str]]:
    catalog_boards = []
    for entry in read_json_url(api_url):
        if entry.get("type") != "file" or not entry.get("name", "").endswith(".json"):
            continue

        board = read_json_url(entry["download_url"])
        mcu = str(board.get("build", {}).get("mcu", "")).lower()
        name = str(board.get("name", "")).strip()
        if mcu not in SOC_LABELS or not name:
            continue

        source_id = entry["name"].removesuffix(".json")
        catalog_boards.append({
            "id": f"{source}:{source_id}",
            "source": source,
            "source_id": source_id,
            "name": name,
            "soc": SOC_LABELS[mcu],
            "mcu": mcu,
            "vendor": str(board.get("vendor", "")).strip(),
            "variant": str(board.get("build", {}).get("variant", "")).strip(),
            "url": str(board.get("url", "")).strip(),
        })
    return catalog_boards


def normalized_identity_name(board: dict[str, str]) -> str:
    name = board["name"].lower().replace("_", " ")
    name = re.sub(r"\besp32([a-z]\d?)\b", r"esp32 \1", name)
    name = re.sub(r"\besp32-([a-z]\d?)\b", r"esp32 \1", name)
    name = re.sub(r"[^a-z0-9]+", " ", name)
    name = " ".join(name.split())

    for prefix in VENDOR_PREFIXES:
        if name == prefix:
            break
        if name.startswith(prefix + " "):
            name = name[len(prefix) + 1:]
            break

    return name


def merge_alias(target: dict[str, object], alias: dict[str, str]) -> None:
    aliases = target.setdefault("aliases", [])
    assert isinstance(aliases, list)
    aliases.append({
        "id": alias["id"],
        "source": alias["source"],
        "source_id": alias["source_id"],
        "name": alias["name"],
    })


def prefer_board(candidate: dict[str, str], current: dict[str, object]) -> bool:
    if candidate["source"].startswith("platformio-") and not str(current["source"]).startswith("platformio-"):
        return True
    if bool(candidate.get("vendor")) and not bool(current.get("vendor")):
        return True
    if bool(candidate.get("url")) and not bool(current.get("url")):
        return True
    return False


def merge_boards(boards: list[dict[str, str]]) -> dict[str, list[dict[str, str]]]:
    families: dict[str, list[dict[str, str]]] = {label: [] for label in SOC_LABELS.values()}
    merged: dict[tuple[str, str], dict[str, object]] = {}

    for board in sorted(boards, key=lambda item: (not item["source"].startswith("platformio-"), item["name"].lower())):
        dedupe_key = (board["soc"], normalized_identity_name(board))
        existing = merged.get(dedupe_key)
        if existing is None:
            merged[dedupe_key] = {**board, "aliases": []}
            continue

        if prefer_board(board, existing):
            replacement = {**board, "aliases": existing["aliases"]}
            merge_alias(replacement, {
                "id": str(existing["id"]),
                "source": str(existing["source"]),
                "source_id": str(existing["source_id"]),
                "name": str(existing["name"]),
            })
            merged[dedupe_key] = replacement
        else:
            merge_alias(existing, board)

    for board in merged.values():
        families[str(board["soc"])].append(board)

    for family_boards in families.values():
        family_boards.sort(key=lambda item: (item["name"].lower(), item["source"], item["source_id"]))

    return families


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate espmark board catalog from PlatformIO and Arduino ESP boards.")
    parser.add_argument(
        "--arduino-boards-txt",
        default=str(Path.home() / ".arduino15/packages/esp32/hardware/esp32/3.3.8/boards.txt"),
        help="Optional path to arduino-esp32 boards.txt",
    )
    parser.add_argument(
        "--arduino-esp8266-boards-txt",
        default=str(Path.home() / ".arduino15/packages/esp8266/hardware/esp8266/3.1.2/boards.txt"),
        help="Optional path to arduino-esp8266 boards.txt",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="web/boards.json",
        help="Output JSON path",
    )
    parser.add_argument(
        "--no-platformio",
        action="store_true",
        help="Skip fetching PlatformIO board registries from GitHub",
    )
    args = parser.parse_args()

    sources = []
    boards = parse_arduino_boards_txt(Path(args.arduino_boards_txt).expanduser(), "arduino-esp32")
    sources.append("arduino-esp32 boards.txt")
    boards.extend(parse_arduino_boards_txt(Path(args.arduino_esp8266_boards_txt).expanduser(), "arduino-esp8266"))
    sources.append("arduino-esp8266 boards.txt")

    if not args.no_platformio:
        boards.extend(parse_platformio_boards())
        boards.extend(parse_platformio_esp8266_boards())
        sources.insert(0, "platformio/platform-espressif32 boards")
        sources.insert(1, "platformio/platform-espressif8266 boards")

    catalog = {
        "schema_version": 1,
        "sources": sources,
        "families": merge_boards(boards),
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
