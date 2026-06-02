# Server HTTP API

## Frames

### `POST /api/frames`

Store a JPEG uploaded by the camera.

Request headers:

| Header | Description |
|--------|-------------|
| `Content-Type: image/jpeg` | JPEG request body |
| `X-Captured-At` | ISO 8601 capture timestamp |
| `X-Trigger` | `timer`, `test`, or `manual` |

Response `201`:

```json
{
  "id": 42,
  "filename": "2026/06/02/14-30-00_timer.jpg"
}
```

Device timestamps more than one day away from server UTC are treated as an
unsynchronized RTC value and replaced with server receive time.

Response headers deliver pending runtime camera settings:

```text
X-Config-Interval: 5
X-Config-Sleep: 1
X-Config-Framesize: UXGA
X-Config-Quality: 12
X-Config-Brightness: 0
X-Config-Contrast: 0
X-Config-Saturation: 0
X-Config-Sharpness: 0
X-Config-Hmirror: 1
X-Config-Vflip: 1
```

### `GET /api/frames`

List stored frames, newest first.

| Query parameter | Default | Description |
|-----------------|---------|-------------|
| `page` | `1` | Page number |
| `per_page` | `50` | Results per page |
| `from_dt` | None | Inclusive ISO 8601 lower bound |
| `to_dt` | None | Inclusive ISO 8601 upper bound |

### `GET /api/frames/{id}`

Return one stored JPEG by frame ID.

## Snapshot

### `GET /api/snapshot`

Try to fetch a fresh JPEG from the camera. If the camera is sleeping or
unreachable, return the newest stored JPEG. Return `503` when neither exists.

The `X-Snapshot-Source` response header is `live` for a fresh device capture
and `cached` for a stored fallback. Cached responses also include
`X-Captured-At`.

## Camera Management

### `GET /api/camera/status`

Proxy the camera's live `GET /status` telemetry endpoint. Returns `503` while
the camera is unavailable.

### `GET /api/camera/config`

Return live camera runtime configuration when available. Fall back to the
server-side cached configuration when the camera is unavailable.

```json
{
  "interval_minutes": 5,
  "sleep_enabled": true,
  "framesize": "UXGA",
  "quality": 12,
  "brightness": 0,
  "contrast": 0,
  "saturation": 0,
  "sharpness": 0,
  "hmirror": true,
  "vflip": true,
  "source": "live"
}
```

`source` is either `live` or `cached`.

### `POST /api/camera/config`

Persist partial runtime camera settings and attempt to push them to the camera
immediately.

```json
{
  "interval_minutes": 5,
  "sleep_enabled": false,
  "contrast": 1,
  "quality": 10
}
```

Response:

```json
{
  "status": "saved",
  "pushed_to_camera": true
}
```

When `pushed_to_camera` is `false`, the server delivers the cached values in
headers on the camera's next `POST /api/frames`.

### `POST /api/camera/preview`

Apply a complete image configuration temporarily on the live camera and return
a newly captured `image/jpeg`. This endpoint does not save server-side config.
It returns `503` when the camera is sleeping or unreachable.

### `GET /api/storage`

Return the number of stored frames and JPEG disk usage.

## OTA

### `POST /api/ota`

Forward an `application/octet-stream` firmware binary to the camera's
`POST /ota` endpoint. Returns `503` if the camera is unreachable and `502` if
the camera rejects the update.

## WebSocket

### `WS /ws/live`

Push a fresh JPEG approximately once per second. Send the text message
`offline` while the camera is unavailable. The browser UI currently polls
`GET /api/snapshot` instead.
