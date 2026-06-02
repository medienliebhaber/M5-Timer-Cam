# Firmware Configuration

## Build-Time Settings

Run `idf.py menuconfig` from `firmware/`. Application settings live under
`M5 TimerCam Configuration`.

| Kconfig key | Checked-in default | Description |
|-------------|--------------------|-------------|
| `M5CAM_WIFI_SSID` | Empty | WiFi network name |
| `M5CAM_WIFI_PASSWORD` | Empty | WiFi password |
| `M5CAM_SERVER_IP` | `192.168.1.100` | FastAPI server address |
| `M5CAM_SERVER_PORT` | `8000` | FastAPI server port |
| `M5CAM_CAPTURE_INTERVAL_MINUTES` | `5` | Capture interval, from 1 to 1440 minutes |
| `M5CAM_TEST_MODE` | Enabled | Capture every two seconds without sleep |

`firmware/sdkconfig.defaults` stores non-secret defaults. Copy it to
`firmware/sdkconfig`, run `idf.py menuconfig`, and keep the generated
`firmware/sdkconfig` file uncommitted.

## Runtime Settings

The camera persists runtime settings in NVS under the `m5cam` namespace.

| Setting | NVS key | Description |
|---------|---------|-------------|
| Capture interval | `interval` | Minutes between scheduled captures |
| Deep sleep | `sleep_en` | `1` to sleep between captures, `0` to stay awake |
| Resolution | `framesize` | `VGA`, `SVGA`, `XGA`, `SXGA`, `UXGA`, or `QXGA` |
| JPEG quality | `quality` | `0` to `63`; lower values produce higher quality |
| Brightness | `brightness` | `-3` to `3` |
| Contrast | `contrast` | `-3` to `3` |
| Saturation | `saturation` | `-4` to `4` |
| Sharpness | `sharpness` | `-3` to `3` |
| Horizontal mirror | `hmirror` | `1` to mirror, `0` for normal orientation |
| Vertical flip | `vflip` | `1` to flip, `0` for normal orientation |

The server can update these values in two ways:

- Immediately through the camera's `POST /config` endpoint while it is awake.
- On the next upload through `X-Config-*` response headers from
  `POST /api/frames`.

The image controls are backed by the
[Espressif esp32-camera driver](https://github.com/espressif/esp32-camera).
The board uses the
[M5Stack TimerCamera-X](https://docs.m5stack.com/en/unit/timercam_x) OV3660
sensor.

See the [camera HTTP API](http-api.md) and
[server HTTP API](../server/http-api.md).
