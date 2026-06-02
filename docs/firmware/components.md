# Firmware Components

Each component lives under `firmware/components/`.

| Component | Responsibility |
|-----------|----------------|
| `camera` | Initialize OV3660 GPIOs and capture JPEG buffers |
| `wifi` | Connect to WiFi in station mode with retries |
| `http_client` | Upload JPEG frames and apply server configuration headers |
| `http_server` | Serve snapshot, status, runtime config, and OTA endpoints |
| `rtc` | Read BM8563 time, configure timed sleep, and release battery hold |
| `led` | Emit named status blink patterns |

## Main Application

`firmware/main/main.c` orchestrates boot, capture, upload, awake operation, test
mode, and scheduled sleep. See the [lifecycle reference](lifecycle.md).
