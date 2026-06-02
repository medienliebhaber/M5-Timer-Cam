# Firmware Guide

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) installed and sourced
- USB-C cable connected to camera

Verify: `idf.py --version` should print `ESP-IDF v5.x.x`

## Build & Flash

```bash
cd firmware

# First time: copy defaults and configure secrets
cp sdkconfig.defaults sdkconfig
idf.py menuconfig
# Navigate to: M5 TimerCam Configuration
# Set: WIFI_SSID, WIFI_PASSWORD, SERVER_IP, SERVER_PORT

# Build and flash
idf.py build flash

# Or use the helper script from repo root
scripts/flash.sh
```

## Monitor Serial Output

```bash
idf.py monitor
# or
scripts/monitor.sh
```

Exit monitor: `Ctrl+]`

## Build Modes

### Normal Mode (default)

Deep-sleep timer loop. The device:
1. Wakes from deep sleep (RTC alarm or first boot)
2. Connects to WiFi
3. Captures a JPEG from OV3660
4. POSTs JPEG to `http://<SERVER_IP>:<SERVER_PORT>/api/frames`
5. Starts HTTP server to answer `GET /snapshot` requests
6. Sets next RTC wake alarm
7. Releases Battery Hold (GPIO33 LOW) → enters deep sleep

### Test Mode

Build with test mode enabled:

```bash
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.test" build flash
```

Or enable via menuconfig: `M5 TimerCam → Enable test mode`.

In test mode:
- No deep sleep — runs continuously
- Captures every 2 seconds
- POSTs each frame to server
- Blinks LED on each capture
- Logs to serial: timestamp, JPEG size, HTTP response

Use this mode for initial hardware validation and pipeline testing.

## Components

Each component lives in `firmware/components/<name>/` with its own `CMakeLists.txt`.

### `camera`

Initializes OV3660 with the correct GPIO map and captures a JPEG into a heap buffer.

```c
esp_err_t camera_init(void);
esp_err_t camera_capture(uint8_t **buf, size_t *len);
void      camera_release(uint8_t *buf);
```

Default capture resolution: UXGA (1600×1200). Configurable via menuconfig.

### `wifi`

Connects the ESP32 to your home network in STA mode. Retries with exponential backoff. Signals connection state via LED codes.

```c
esp_err_t wifi_connect(void);   // blocks until connected or max retries
void      wifi_disconnect(void);
bool      wifi_is_connected(void);
```

### `http_client`

POSTs a JPEG buffer to the server's `/api/frames` endpoint. Sends `X-Captured-At` (ISO8601) and `X-Trigger` (`timer` or `test`) headers.

```c
esp_err_t http_client_post_frame(const uint8_t *buf, size_t len,
                                  const char *trigger);
```

### `http_server`

Starts a minimal HTTP server on port 80. Answers `GET /snapshot` with a fresh JPEG capture. Used by the FastAPI server's pull mode.

```c
esp_err_t http_server_start(void);
void      http_server_stop(void);
```

### `rtc`

Reads current time from BM8563 over I2C (GPIO14/12). Sets a deep-sleep wake alarm for the next capture interval.

```c
esp_err_t rtc_init(void);
esp_err_t rtc_get_time(struct tm *t);
esp_err_t rtc_set_wake_alarm(int minutes_from_now);
```

### `led`

Controls the onboard LED (GPIO2) with named blink patterns.

```c
void led_blink(led_pattern_t pattern);
```

| Pattern | Meaning |
|---------|---------|
| `LED_WIFI_CONNECTING` | Fast blink (100ms) |
| `LED_CAPTURE_OK` | 2 quick pulses |
| `LED_ERROR` | 5 rapid flashes |
| `LED_SLEEPING` | Single long pulse then off |

## Configuration Reference (Kconfig)

All settings live under `M5 TimerCam Configuration` in menuconfig.

| Key | Default | Description |
|-----|---------|-------------|
| `WIFI_SSID` | — | Your WiFi network name |
| `WIFI_PASSWORD` | — | Your WiFi password |
| `SERVER_IP` | `192.168.1.100` | IP of the machine running the FastAPI server |
| `SERVER_PORT` | `8000` | FastAPI server port |
| `CAPTURE_INTERVAL_MINUTES` | `5` | Minutes between timer captures |
| `CAPTURE_FRAMESIZE` | `UXGA` | OV3660 resolution (VGA/SVGA/XGA/SXGA/UXGA/QXGA) |
| `TEST_MODE` | `n` | Enable continuous capture mode |

Secrets (`WIFI_SSID`, `WIFI_PASSWORD`) are stored only in `sdkconfig` which is gitignored.
