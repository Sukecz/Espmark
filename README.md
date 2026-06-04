# espmark

Community benchmark sketch for ESP32-family boards.

The first public milestone focuses on CPU measurements that are easy to run on
many boards without extra fixtures. Users compile the Arduino sketch for their
own board, upload it, and copy the JSON result printed to Serial Monitor between
`ESPMARK_RESULT_BEGIN` and `ESPMARK_RESULT_END` markers.

## Current status

- Arduino IDE starter sketch
- Initial CPU integer microbenchmarks
- Board and SoC manifests
- JSON result schema draft
- Host helper for collecting UART output

The supported path is Arduino IDE. Users select their own ESP32 board, compile,
upload, and submit the generated JSON result.

## Arduino IDE quick start

1. Open `arduino/espmark/espmark.ino` in Arduino IDE.
2. Select your ESP32 board. For the first test board use `XIAO_ESP32C6`.
3. Upload.
4. Open Serial Monitor at `115200` baud.
5. Copy the JSON between `ESPMARK_RESULT_BEGIN` and `ESPMARK_RESULT_END`.

No ESP-IDF or Docker setup is required for normal users.

## Collect one result

```bash
python3 harness/collect_serial.py --port /dev/ttyACM0 --baud 115200 --out results/xiao-c6.json
```

The output JSON is intended to become the stable input format for the future
community result registry.

## Project layout

```text
arduino/              Arduino IDE starter sketch
assets/               Project logos and static assets
boards/               Board manifests
harness/              Host-side collection tooling
soc_caps/             SoC capability manifests
schemas/              JSON schemas for manifests and result bundles
espmark.md            Long-form project notes and roadmap
```

## Roadmap

1. CPU baseline and result schema
2. RAM and flash benchmarks
3. RTOS latency and boot/wake timing
4. Board manifest submissions
5. Community web registry with moderation

## License

Apache-2.0. See `LICENSE`.
