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

Copy `.env.example` to `.env` and fill in your values:

```bash
cp .env.example .env
```

| Variable | Default | Description |
|----------|---------|-------------|
| `CAMERA_IP` | `192.168.4.1` | Camera IP on your local network |
| `CAMERA_PORT` | `80` | Camera HTTP server port |
| `DATA_DIR` | `../data` | Root for images + SQLite DB |
| `PORT` | `8000` | Server listen port |

## API Reference

### `POST /api/frames`

Receive a JPEG from the camera. Called by firmware after each capture.

**Headers:**
- `Content-Type: image/jpeg`
- `X-Captured-At: 2026-06-02T14:30:00Z` (ISO8601 UTC)
- `X-Trigger: timer | test`

**Response:** `201 Created`
```json
{ "id": 42, "filename": "2026/06/02/14-30-00_timer.jpg" }
```

---

### `GET /api/frames`

List stored frames, newest first.

**Query params:**
- `page` (int, default 1)
- `per_page` (int, default 50)
- `from` (ISO8601 date, optional)
- `to` (ISO8601 date, optional)

**Response:** `200 OK`
```json
{
  "total": 123,
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

### `GET /api/frames/{id}`

Serve the JPEG file for a given frame ID.

**Response:** `200 OK` with `Content-Type: image/jpeg`

---

### `GET /api/snapshot`

Pull a live JPEG from the camera right now. Proxies to `http://<CAMERA_IP>/snapshot`.

**Response:** `200 OK` with `Content-Type: image/jpeg`

Returns `503 Service Unavailable` if camera is unreachable.

---

### `WS /ws/live`

WebSocket that pushes live JPEG frames as binary messages at ~1fps.

The server polls `GET /api/snapshot` internally and forwards each frame to all connected clients. If the camera goes offline, the WebSocket stays open and resumes when it comes back.

---

### `GET /`

Serves `web/index.html`.

## Database Schema

SQLite database at `data/camera.db`.

```sql
CREATE TABLE frames (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    captured_at TEXT    NOT NULL,   -- ISO8601 UTC
    trigger     TEXT    NOT NULL,   -- "timer" | "test" | "manual"
    filename    TEXT    NOT NULL,   -- relative path under DATA_DIR/images/
    filesize    INTEGER,            -- bytes
    width       INTEGER,            -- pixels
    height      INTEGER             -- pixels
);

CREATE INDEX idx_frames_captured_at ON frames (captured_at DESC);
```

## Image File Layout

```
data/
└── images/
    └── YYYY/
        └── MM/
            └── DD/
                └── HH-MM-SS_{trigger}.jpg
```

Example: `data/images/2026/06/02/14-30-00_timer.jpg`

## Running Tests

```bash
cd server
pytest                    # all tests
pytest -v tests/          # verbose
pytest tests/test_frames_api.py  # single file
```

Tests use an in-memory SQLite DB and a temporary directory for images — no real camera required.
