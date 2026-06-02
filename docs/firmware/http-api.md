# Camera HTTP API

The camera exposes these endpoints on port `80` while its HTTP server is
running. In scheduled sleep mode, they are available only during the short
awake window.

## `GET /snapshot`

Capture and return a fresh JPEG.

**Response:** `image/jpeg`

## `GET /status`

Return live device telemetry.

```json
{
  "heap_free": 198432,
  "heap_min": 154320,
  "rssi": -62,
  "battery_pct": 78,
  "battery_mv": 4010
}
```

## `GET /config`

Return runtime camera settings stored in NVS.

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
  "vflip": true
}
```

## `POST /config`

Update any runtime settings. Image values are applied to the OV3660 and saved
to NVS. The interval range is 1 to 1440 minutes.

```json
{
  "interval_minutes": 5,
  "sleep_enabled": false
}
```

## `POST /preview`

Apply image settings to the real OV3660 sensor without saving them to NVS, then
capture and return a fresh `image/jpeg`. The web UI uses this endpoint through
the server proxy while the camera is awake.

## `POST /power-off`

Disable scheduled wake, release GPIO33, and enter timerless ESP32 deep sleep.
The camera remains inactive until USB is unplugged and reconnected or a
hardware wake/reset occurs.

## `POST /ota`

Write an `application/octet-stream` firmware binary to the inactive OTA
partition, mark it bootable, and restart the device.

See the [partition table](partitions.md) and the
[server OTA proxy](../server/http-api.md#post-apiota).
