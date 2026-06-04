# espmark

Lightweight community benchmark for ESP32-family boards.

The goal is to make ESP32 board comparisons easy for normal users: connect a
board over USB, flash or run espmark, collect the result in a browser, and later
submit it to a public results table.

## Current Direction

The main user path is the web UI:

```text
https://espmark.msmeteo.cz
```

The web page uses browser Web Serial to talk to the board over USB. This avoids
asking users for Wi-Fi credentials and keeps network activity outside the
benchmark itself.

For now, firmware is still uploaded manually from Arduino IDE. The next major
step is web flashing with prebuilt firmware binaries.

## Project Goals

- Keep the benchmark simple enough for community use.
- Avoid ESP-IDF/Docker requirements for normal users.
- Prefer USB/Web Serial over Wi-Fi upload for trust and repeatability.
- Print human-readable benchmark results on serial.
- Keep JSON export for automated collection and future web submissions.
- Grow tests gradually: CPU first, then memory, flash, boot/RTOS and board
  metadata.

## User Flow Today

1. Open `arduino/espmark/espmark.ino` in Arduino IDE.
2. Select the board, currently tested with `XIAO_ESP32C6`.
3. Upload the sketch.
4. Open `https://espmark.msmeteo.cz` in Chrome or Edge.
5. Click `Connect USB`.
6. When the board asks for Enter, click `Start benchmark`.
7. Click `Request JSON`.
8. Click `Save Result` to store it in the local browser table.

Web Serial requires HTTPS. The LAN URL can show the page, but browser serial
access should be tested through `https://espmark.msmeteo.cz`.

## Arduino Fallback

Users can still run without the web page:

1. Open Serial Monitor at `115200`.
2. Press Enter to start the benchmark.
3. Read the table.
4. Press `j` then Enter to print JSON.

## Project Layout

```text
arduino/espmark/espmark.ino      Benchmark firmware source
web/                             Static web UI
web/assets/espmarklogo_big.png   Logo used by the web UI
web/firmware/manifest.json       Placeholder for future web-flash manifest
assets/                          Source logo assets
.github/workflows/               Arduino compile check
```

## Runtime Deployment

The web is served from RPI:

```text
/home/msrpi/espmark
```

Docker compose service on RPI:

```text
espmark-web
```

Local LAN URL:

```text
http://192.168.1.4:8095/
```

Public HTTPS URL through Cloudflare Tunnel:

```text
https://espmark.msmeteo.cz
```

Cloudflare tunnel route:

```text
Hostname: espmark.msmeteo.cz
Service type: HTTP
Service URL: localhost:8095
```

## Updating Firmware Code

When editing `arduino/espmark/espmark.ino`:

1. Keep serial commands stable where possible:
   - Enter starts/re-runs benchmark.
   - `j` + Enter prints JSON.
2. Keep JSON markers stable:
   - `ESPMARK_RESULT_BEGIN`
   - `ESPMARK_RESULT_END`
3. Compile for the current test board:

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 arduino/espmark
```

4. If JSON fields change, update `web/app.js` parsing/rendering.
5. Re-test through `https://espmark.msmeteo.cz`.

## Updating The Web

Edit files under `web/`.

Local checks:

```bash
node --check web/app.js
```

Deploy to RPI:

```bash
rsync -av --delete web/ rpi5:/home/msrpi/espmark/
```

The nginx container uses a bind mount, so static file changes do not require a
container restart.

Verify from RPI:

```bash
ssh rpi5 'curl -fsS http://127.0.0.1:8095/ | head'
```

## Updating Docker Compose

RPI compose file:

```text
/home/msrpi/docker-com/docker-compose.yml
```

Before changing it, make a backup. After changing it, validate:

```bash
ssh rpi5 'docker compose -f /home/msrpi/docker-com/docker-compose.yml config --quiet'
```

Start or update only this service:

```bash
ssh rpi5 'docker compose -f /home/msrpi/docker-com/docker-compose.yml up -d espmark-web'
```

Do not restart the whole compose stack unless explicitly needed.

## Roadmap

1. Improve current web serial flow and result display.
2. Add prebuilt firmware binaries for XIAO ESP32C6.
3. Add ESP Web Tools flashing from the website.
4. Add persistent backend submissions instead of local browser storage.
5. Add public result table and compare view.
6. Add more boards and SoC families.
7. Add memory and flash benchmarks.

## License

Apache-2.0. See `LICENSE`.
