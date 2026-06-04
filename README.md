# espmark

Community benchmark firmware for ESP32-family boards.

The first public milestone focuses on CPU measurements that are easy to run on
many boards without extra fixtures. The firmware prints a JSON result bundle to
UART between `ESPMARK_RESULT_BEGIN` and `ESPMARK_RESULT_END` markers.

## Current status

- ESP-IDF project skeleton
- Initial CPU integer microbenchmarks
- Board and SoC manifests
- JSON result schema draft
- Host helper for collecting UART output

The default configuration is prepared for Seeed Studio XIAO ESP32C6
(`esp32c6` target).

## Build and flash

Install ESP-IDF, then from this repository:

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Use the correct serial port for your machine. The XIAO ESP32C6 usually exposes
native USB serial/JTAG, but the actual device path depends on the host.

## Collect one result

```bash
python3 harness/collect_serial.py --port /dev/ttyACM0 --baud 115200 --out results/xiao-c6.json
```

The output JSON is intended to become the stable input format for the future
community result registry.

## Project layout

```text
main/                 ESP-IDF firmware
boards/               Board manifests
soc_caps/             SoC capability manifests
schemas/              JSON schemas for manifests and result bundles
harness/              Host-side collection tooling
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

