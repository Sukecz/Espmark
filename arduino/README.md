# Arduino IDE starter

This folder contains the easiest first-run version of espmark.

## Run

1. Install Arduino IDE.
2. Install the `esp32` board package by Espressif.
3. Open `arduino/espmark/espmark.ino`.
4. Select your board, for example `XIAO_ESP32C6`.
5. Upload.
6. Open Serial Monitor at `115200` baud.

Copy the JSON printed between:

```text
ESPMARK_RESULT_BEGIN
ESPMARK_RESULT_END
```

This is the main espmark firmware path for now. Users compile it for their own
board in Arduino IDE and submit the generated JSON result.
