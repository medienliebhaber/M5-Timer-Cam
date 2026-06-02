# M5 Timer Camera X — Project Overview

## What This Is

ESP32-based timer camera that captures JPEGs on a schedule, POSTs them to a local FastAPI server, and serves a live view + gallery UI in the browser.

## Architecture

```
[M5 TimerCam X]  ──POST /api/frames──▶  [FastAPI server]  ──▶  [SQLite + filesystem]
(ESP32, OV3660)  ◀──GET /snapshot────   (Python)                data/images/ + camera.db
                                                │
                                         [Web browser]
                                         /  (live + gallery)
```

**Two delivery modes:**
- **Timer push** — camera wakes from deep sleep, captures JPEG, POSTs to server, sleeps again
- **Live pull** — server requests a fresh snapshot from the camera on demand

## Quick Start

```bash
# 1. Server
cd server && ./scripts/setup.sh
uvicorn app.main:app --reload          # http://localhost:8000

# 2. Firmware
cd firmware
cp sdkconfig.defaults sdkconfig        # then edit WIFI/SERVER settings via idf.py menuconfig
../scripts/flash.sh                    # build + flash

# 3. Test
../scripts/test_camera.sh              # smoke test full pipeline
```

## Repository Layout

| Path | Purpose |
|------|---------|
| `firmware/` | ESP-IDF project — runs on the camera |
| `server/` | FastAPI backend — ingest, storage, API |
| `web/` | Static frontend — live view + gallery |
| `scripts/` | Dev helpers: setup, flash, monitor, test |
| `docs/` | All detailed documentation |
| `data/` | Gitignored — images + SQLite DB |

## Key Documentation

- [docs/hardware.md](docs/hardware.md) — GPIO map, pinout, wiring
- [docs/firmware.md](docs/firmware.md) — Build, flash, components, test mode
- [docs/server.md](docs/server.md) — API reference, DB schema, config
- [docs/development.md](docs/development.md) — Full dev workflow end-to-end

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Firmware | ESP-IDF (native C), ESP32-D0WDQ6-V3 |
| Camera | OV3660, up to 3MP |
| Server | Python 3.12+, FastAPI, SQLite, httpx |
| Frontend | Vanilla HTML/JS, no build step |

## AI Assistant Notes

- All design decisions are documented in `docs/superpowers/specs/`
- Each major component has its own doc in `docs/`
- `sdkconfig.defaults` captures all non-secret firmware settings
- Secret values (WiFi password, server IP) live only in `sdkconfig` (gitignored) and `.env` (gitignored)
