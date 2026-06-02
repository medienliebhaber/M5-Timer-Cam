# Architecture

The M5 Timer Camera X captures JPEG images and sends them to a local FastAPI
server. The server stores image metadata in SQLite, writes JPEG files to disk,
and serves the browser UI.

```text
M5 TimerCam X               FastAPI server                  Browser
ESP32 + OV3660              localhost:8000

POST /api/frames  ------->  SQLite + data/images/
GET /snapshot     <-------  GET /api/snapshot  <---------- live view
GET/POST /config  <-------  /api/camera/config <---------- settings
GET /status       <-------  /api/camera/status <---------- telemetry
POST /ota         <-------  POST /api/ota
                            GET /api/frames    <---------- gallery/playback
```

## Capture Modes

| Mode | Behavior |
|------|----------|
| Scheduled sleep | Connect WiFi, capture, upload, briefly serve camera endpoints, then release battery hold and sleep |
| Awake | Keep WiFi and camera HTTP endpoints available; capture at the configured interval |
| Test | Stay awake and capture every two seconds for hardware validation |

## Storage

The server stores metadata in `data/camera.db`, JPEG files under
`data/images/`, and the last server-side runtime camera configuration in
`data/camera_config.json`.

## References

- [Firmware lifecycle](firmware/lifecycle.md)
- [Camera-hosted HTTP API](firmware/http-api.md)
- [Server HTTP API](server/http-api.md)
- [Database and image layout](server/database.md)
