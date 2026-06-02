# Development Workflow

## Prerequisites

Install these once:

```bash
# ESP-IDF v5.x — follow official guide:
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# Python 3.12+
brew install python@3.12

# uv (fast Python package manager)
curl -Ls https://astral.sh/uv/install.sh | sh

# GitHub CLI
brew install gh
gh auth login
```

## First-Time Setup

```bash
git clone https://github.com/maskow/M5-Timer-Cam
cd M5-Timer-Cam

# Server setup
cd server
../scripts/setup.sh
cd ..

# Firmware config
cd firmware
idf.py menuconfig
# Set: WIFI_SSID, WIFI_PASSWORD, SERVER_IP, SERVER_PORT
cd ..
```

## Day-to-Day Development

### Running the server

```bash
cd server
uvicorn app.main:app --reload
```

UI at [http://localhost:8000](http://localhost:8000)
API docs at [http://localhost:8000/docs](http://localhost:8000/docs) (FastAPI auto-generated)

### Firmware development loop

```bash
# Edit firmware source
# Then:
scripts/flash.sh        # build + flash (takes ~30s on first build, ~5s incremental)
scripts/monitor.sh      # attach serial monitor

# Or one command:
cd firmware && idf.py build flash monitor
```

### Test the full pipeline

```bash
scripts/test_camera.sh
```

This:
1. Pings the camera
2. Pulls a live snapshot
3. POSTs a test frame to the server
4. Verifies the frame appears in the API

Requires: camera on network, server running.

## Testing

### Server tests (no camera needed)

```bash
cd server
pytest -v
```

### Firmware test mode

Flash with test mode to run a continuous capture loop (no deep sleep):

```bash
cd firmware
# Enable test mode in menuconfig: M5 TimerCam → Enable test mode
idf.py build flash monitor
```

Watch serial output — should see `CAPTURE OK` and `POST OK` every 2 seconds.
Check the Gallery tab in the browser to confirm frames are arriving.

## Making Changes

### Adding a new firmware component

1. Create `firmware/components/<name>/` with `CMakeLists.txt` and source files
2. Add `REQUIRES <name>` to `firmware/main/CMakeLists.txt`
3. Add Kconfig options if needed
4. Document in `docs/firmware.md`

### Adding a new API endpoint

1. Create or edit a route file in `server/app/api/`
2. Register the router in `server/app/main.py`
3. Add tests in `server/tests/`
4. Update `docs/server.md`

### Changing the DB schema

SQLite migrations are run at startup by `server/app/storage/db.py`. Add new `ALTER TABLE` statements there — they are idempotent (wrapped in `IF NOT EXISTS` checks).

## Git Workflow

```bash
git checkout -b feature/my-change
# make changes
git add <specific files>
git commit -m "feat: description of what changed"
git push -u origin feature/my-change
gh pr create
```

Commit types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

## Troubleshooting

| Symptom | Check |
|---------|-------|
| `idf.py: command not found` | Source IDF: `. $HOME/esp/esp-idf/export.sh` |
| Camera not reachable | Check WiFi credentials in `sdkconfig`, verify camera LED |
| Server can't reach camera | Check `CAMERA_IP` in `.env`, ping from server machine |
| Frames not appearing | Check `scripts/monitor.sh` for HTTP POST errors |
| DB errors on startup | Delete `data/camera.db` — it will be recreated |
