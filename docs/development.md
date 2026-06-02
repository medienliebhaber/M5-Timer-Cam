# Development Workflow

Complete [setup](setup.md) before using this guide.

## Run the Server

```bash
cd server
uvicorn app.main:app --reload
```

- UI: [http://localhost:8000](http://localhost:8000)
- Generated API docs: [http://localhost:8000/docs](http://localhost:8000/docs)

## Firmware Loop

From the repository root:

```bash
scripts/flash.sh
scripts/monitor.sh
```

Exit the serial monitor with `Ctrl+]`.

For lifecycle and configuration details, see the
[firmware reference](firmware/README.md).

## Smoke Test

With the server running and the camera awake:

```bash
scripts/test_camera.sh
```

The script checks camera reachability, fetches a live snapshot, posts a test
frame to the server, and verifies that the frame appears in the gallery API.

## Automated Tests

Server tests do not require a camera:

```bash
cd server
pytest -v
```

## Change Checklist

When adding a firmware feature:

1. Update the relevant file under `docs/firmware/`.
2. Update [hardware.md](hardware.md) only when board or GPIO facts change.
3. Run the firmware build and relevant hardware validation.

When adding a server endpoint:

1. Add or update tests under `server/tests/`.
2. Update [server/http-api.md](server/http-api.md).
3. Run `cd server && pytest -v`.

When changing UI behavior:

1. Update [web-ui.md](web-ui.md).
2. Check the affected desktop and mobile flows in the browser.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| `idf.py: command not found` | Source ESP-IDF: `. $HOME/esp/esp-idf/export.sh` |
| Camera not reachable | Check WiFi settings in `firmware/sdkconfig` and `CAMERA_IP` in `server/.env` |
| Frames not appearing | Run `scripts/monitor.sh` and inspect upload errors |
| Live view shows an old image | The camera may be sleeping; `/api/snapshot` falls back to the latest stored frame |
| Server database errors | Stop the server, remove `data/camera.db`, and restart to recreate it |
| OTA update fails | Keep the camera awake and verify the [partition table](firmware/partitions.md) |
