# espmark

Lightweight community benchmark sketch for ESP32-family boards.

The first version measures basic CPU performance. Users compile the Arduino
sketch for their own ESP32 board, upload it, and copy the JSON result printed to
Serial Monitor.

## Quick Start

1. Install Arduino IDE.
2. Install the `esp32` board package by Espressif.
3. Open `arduino/espmark/espmark.ino`.
2. Select your ESP32 board. For the first test board use `XIAO_ESP32C6`.
3. Upload.
4. Open Serial Monitor at `115200` baud.
5. Copy the JSON between `ESPMARK_RESULT_BEGIN` and `ESPMARK_RESULT_END`.

No ESP-IDF or Docker setup is required for normal users.

## Output

```text
ESPMARK_RESULT_BEGIN
{
  ...
}
ESPMARK_RESULT_END
```

The JSON output is the result users will submit once the community results page
exists.

## Project layout

```text
arduino/              Arduino IDE starter sketch
assets/               Project logos and static assets
```

## Roadmap

1. CPU baseline
2. RAM and flash benchmarks
3. Board metadata in the sketch output
4. Community result submission format
5. Community web registry

## License

Apache-2.0. See `LICENSE`.
