# Server Guide

## Prerequisites

- Python 3.12+
- `uv` (recommended) or `pip`

## Setup

```bash
cd server
../scripts/setup.sh    # creates .env, installs deps, inits DB
```

## Running

```bash
# Development (auto-reload)
uvicorn app.main:app --reload

# Production
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

Open [http://localhost:8000](http://localhost:8000) for the UI.

## Configuration (`.env`)

| Variable | Default | Description |
|----------|---------|-------------|
| `CAMERA_IP` | `192.168.4.1` | Camera IP on your local network |
| `CAMERA_PORT` | `80` | Camera HTTP server port |
| `DATA_DIR` | `../data` | Root for images + SQLite DB |
| `PORT` | `8000` | Server listen port |

---

## API Reference

### Frames

#### `POST /api/frames`
Ingest a JPEG from the camera. Called by firmware after each capture.

**Request headers:**
```
Content-Type: image/jpeg
X-Captured-At: 2026-06-02T14:30:00Z   (ISO8601 UTC)
X-Trigger: timer | test | manual
```

**Response `201`:**
```json
{ "id": 42, "filename": "2026/06/02/14-30-00_timer.jpg" }
```

**Response headers** carry pending configuration for the camera to apply:
```
X-Config-Interval: 5       (minutes)
X-Config-Sleep: 1          (1 = sleep enabled, 0 = stay awake)
```

---

#### `GET /api/frames`
List stored frames, newest first.

**Query params:**

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `page` | int | 1 | Page number |
| `per_page` | int | 50 | Results per page |
| `from_dt` | ISO8601 string | — | Start of time range (inclusive) |
| `to_dt` | ISO8601 string | — | End of time range (inclusive) |

**Response `200`:**
```json
{
  "total": 1072,
  "page": 1,
  "per_page": 50,
  "frames": [
    {
      "id": 42,
      "captured_at": "2026-06-02T14:30:00Z",
      "trigger": "timer",
      "filename": "2026/06/02/14-30-00_timer.jpg",
      "filesize": 48210,
      "width": 1600,
      "height": 1200
    }
  ]
}
```

---

#### `GET /api/frames/{id}`
Serve the JPEG for a specific frame.

**Response `200`:** `image/jpeg`

---

### Live snapshot

#### `GET /api/snapshot`
Returns the latest JPEG. Tries the live camera first (`GET /snapshot` on the camera); if the camera is unreachable (sleeping), falls back to the most recently stored frame.

**Response `200`:** `image/jpeg`  
**Response `503`:** camera offline and no stored frames exist

---

### WebSocket

#### `WS /ws/live`
Pushes live JPEG frames as binary messages at ~1 fps. Sends the text `"offline"` when the camera is unreachable (keeps the socket alive). The UI does not use this endpoint — it polls `/api/snapshot` directly — but it is available for other clients.

---

### Camera management

#### `GET /api/camera/status`
Proxy to the camera's `GET /status` endpoint.

**Response `200`:**
```json
{
  "heap_free": 198432,
  "heap_min": 154320,
  "rssi": -62,
  "battery_pct": 78,
  "battery_mv": 4010
}
```

**Response `503`:** camera offline

---

#### `GET /api/camera/config`
Returns the current camera configuration. Uses live data from the camera if it is online; otherwise returns the last saved value from `data/camera_config.json`.

**Response `200`:**
```json
{
  "interval_minutes": 1,
  "sleep_enabled": true,
  "source": "live" | "cached"
}
```

---

#### `POST /api/camera/config`
Save camera configuration. Persists to `data/camera_config.json` and attempts an immediate push to the camera's `POST /config` endpoint.

**Request body:**
```json
{
  "interval_minutes": 5,
  "sleep_enabled": false
}
```

Both fields are optional; omitted fields retain their current value.

**Response `200`:**
```json
{ "status": "saved", "pushed_to_camera": true | false }
```

`pushed_to_camera: false` means the camera was offline — the new config will be delivered via `X-Config-*` headers on the camera's next frame POST.

---

#### `GET /api/storage`
Server-side storage statistics.

**Response `200`:**
```json
{
  "frame_count": 1072,
  "disk_used_bytes": 108891478,
  "disk_used_mb": 103.8
}
```

---

### OTA firmware update

#### `POST /api/ota`
Receive a firmware `.bin` and forward it to the camera's `POST /ota` endpoint. The camera writes the binary to its inactive OTA partition, marks it bootable, and reboots.

**Request:** `application/octet-stream` body (the compiled `.bin` file)  
**Response `200`:** `{ "status": "ok", "message": "OK" }`  
**Response `503`:** camera unreachable  
**Response `502`:** camera rejected the firmware

---

## Database Schema

SQLite at `data/camera.db`.

```sql
CREATE TABLE frames (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    captured_at TEXT    NOT NULL,   -- ISO8601 UTC
    trigger     TEXT    NOT NULL,   -- "timer" | "test" | "manual"
    filename    TEXT    NOT NULL,   -- relative path under DATA_DIR/images/
    filesize    INTEGER,
    width       INTEGER,
    height      INTEGER
);

CREATE INDEX idx_frames_captured_at ON frames (captured_at DESC);
```

Pending camera config is stored as JSON at `data/camera_config.json`.

## Image File Layout

```
data/
└── images/
    └── YYYY/MM/DD/HH-MM-SS_{trigger}.jpg
```

## Running Tests

```bash
cd server && pytest
```

Tests use an in-memory SQLite DB and a temp directory — no real camera required.
