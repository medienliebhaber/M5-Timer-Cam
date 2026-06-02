# M5 Timer Camera X

A self-contained system for scheduled image capture, live preview, and playback using the [M5Stack Timer Camera X](https://docs.m5stack.com/en/unit/timercam_x).

## Features

- **Timer capture** — camera wakes from deep sleep at configurable intervals, captures a JPEG, uploads it to your server, and sleeps again (battery life > 1 month at hourly intervals)
- **Live view** — pull a fresh snapshot from the camera on demand in the browser
- **Gallery + playback** — browse all captured frames with timestamps and date filtering
- **Test mode** — continuous capture every 2s with serial logging, for initial setup and debugging

## Hardware

| Spec | Value |
|------|-------|
| MCU | ESP32-D0WDQ6-V3 |
| Camera | OV3660, 3MP max (2048×1536) |
| PSRAM | 8MB |
| Flash | 4MB |
| Battery | 140mAh, ~2µA sleep |
| RTC | BM8563 |
| Connectivity | WiFi 802.11 b/g/n |

## Requirements

**Firmware:**
- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

**Server:**
- Python 3.12+

**Both on the same WiFi network.**

## Setup

```bash
# Clone
git clone https://github.com/maskow/M5-Timer-Cam
cd M5-Timer-Cam

# Server
cd server
../scripts/setup.sh          # installs deps, creates .env, inits DB
uvicorn app.main:app --reload

# Firmware
cd firmware
idf.py menuconfig             # set WIFI_SSID, WIFI_PASSWORD, SERVER_IP
../../scripts/flash.sh        # build + flash

# Smoke test (camera must be on network)
scripts/test_camera.sh
```

Open [http://localhost:8000](http://localhost:8000) to see the UI.

## Project Structure

```
firmware/     ESP-IDF project (C)
server/       FastAPI backend (Python)
web/          Static frontend (HTML/JS)
scripts/      Dev helpers
docs/         Documentation
data/         Runtime data — gitignored
```

See [CLAUDE.md](CLAUDE.md) for the full architecture overview.

## Documentation

- [Hardware & Pinout](docs/hardware.md)
- [Firmware Guide](docs/firmware.md)
- [Server API & Config](docs/server.md)
- [Development Workflow](docs/development.md)
