#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="$ROOT_DIR/arduino/espmark"
SKETCH_FILE="$SKETCH_DIR/espmark.ino"
FIRMWARE_DIR="$ROOT_DIR/web/firmware"

version="$(sed -nE 's/^#define ESPMARK_VERSION "([^"]+)".*/\1/p' "$SKETCH_FILE")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Could not read semantic ESPMARK_VERSION from $SKETCH_FILE" >&2
  exit 1
fi

suffix="v${version//./}"
mkdir -p "$FIRMWARE_DIR"

targets=(
  "ESP8266|esp8266:esp8266:generic|esp8266"
  "ESP32|esp32:esp32:esp32|esp32"
  "ESP32-C3|esp32:esp32:esp32c3:CDCOnBoot=cdc|esp32c3"
  "ESP32-C5|esp32:esp32:esp32c5:CDCOnBoot=cdc|esp32c5"
  "ESP32-C6|esp32:esp32:esp32c6:CDCOnBoot=cdc|esp32c6"
  "ESP32-S2|esp32:esp32:esp32s2:CDCOnBoot=cdc|esp32s2"
  "ESP32-S3|esp32:esp32:esp32s3:CDCOnBoot=cdc|esp32s3"
  "ESP32-P4|esp32:esp32:esp32p4:CDCOnBoot=cdc|esp32p4"
)

rm -f "$FIRMWARE_DIR"/espmark-*-generic-v*.bin

for entry in "${targets[@]}"; do
  IFS="|" read -r chip_family fqbn short_name <<< "$entry"
  build_path="/tmp/espmark-build-$short_name"
  output_file="$FIRMWARE_DIR/espmark-$short_name-generic-$suffix.bin"

  rm -rf "$build_path"
  echo "== Building $short_name ($fqbn) =="
  arduino-cli compile --fqbn "$fqbn" --build-path "$build_path" --export-binaries "$SKETCH_DIR"
  if [[ -f "$build_path/espmark.ino.merged.bin" ]]; then
    cp "$build_path/espmark.ino.merged.bin" "$output_file"
  else
    cp "$build_path/espmark.ino.bin" "$output_file"
  fi
  ls -lh "$output_file"
done

{
  printf '{\n'
  printf '  "name": "espmark",\n'
  printf '  "version": "%s",\n' "$version"
  printf '  "new_install_prompt_erase": true,\n'
  printf '  "builds": [\n'
  for i in "${!targets[@]}"; do
    IFS="|" read -r chip_family _ short_name <<< "${targets[$i]}"
    comma=","
    if [[ "$i" -eq "$((${#targets[@]} - 1))" ]]; then
      comma=""
    fi
    printf '    {\n'
    printf '      "chipFamily": "%s",\n' "$chip_family"
    printf '      "parts": [\n'
    printf '        { "path": "espmark-%s-generic-%s.bin", "offset": 0 }\n' "$short_name" "$suffix"
    printf '      ]\n'
    printf '    }%s\n' "$comma"
  done
  printf '  ]\n'
  printf '}\n'
} > "$FIRMWARE_DIR/manifest.json"

python3 -m json.tool "$FIRMWARE_DIR/manifest.json" >/dev/null
echo "Wrote $FIRMWARE_DIR/manifest.json for espmark $version ($suffix)"
