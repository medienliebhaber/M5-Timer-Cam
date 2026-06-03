# Firmware Lifecycle

## Boot

On boot the firmware initializes NVS, the BM8563 clock interface, the camera,
and the status LED.

USB power presence is the only switch between the two operating modes. Charging
is inferred from battery voltage above 4.1 V. There is no runtime sleep toggle:
plug in USB to keep the device awake and interactive; on battery it always
captures once and powers off.

## Battery Mode (USB disconnected)

When test mode is disabled and USB is not connected:

1. Connect to WiFi.
2. Initialize the camera with a single framebuffer (`FB_COUNT_ONESHOT`). The
   sensor is powered on only after WiFi associates, not during the connect
   window.
3. Capture a JPEG and post it to the server.
4. Apply runtime configuration returned in `X-Config-*` response headers
   (including a server-requested power-off).
5. Configure the BM8563 countdown timer and the ESP32 USB-power fallback timer.
6. Release battery hold on GPIO33 and enter deep sleep.

There is no live HTTP-server window on battery. Settings changed from the web UI
are stashed server-side and delivered to the camera as `X-Config-*` headers on
its next scheduled wake.

The BM8563 retains wall-clock time for capture timestamps. Its timer interrupt
powers the board back on after the configured interval when battery-powered.
The ESP32 deep-sleep timer wakes the board when USB power remains connected.

Manual web UI shutdown is distinct from scheduled sleep. It disables the
BM8563 countdown timer and stale timer interrupt state, releases GPIO33, and
enters ESP32 timerless deep sleep. The camera remains inactive until USB is
unplugged and reconnected or a hardware wake/reset occurs.

## Awake Mode (USB connected)

When USB charging is inferred, the camera never sleeps. It initializes the
camera with two framebuffers (`FB_COUNT_STREAM`) for smooth streaming, keeps
WiFi and the camera HTTP server available, and captures at the configured
interval. The web UI can poll `/snapshot` (~1×/s) for live view.

If USB is disconnected while awake, the firmware restarts and returns to the
battery power-off flow.

## Test Mode

Test mode is enabled in the checked-in `sdkconfig.defaults` for initial
validation. The camera stays awake, starts its HTTP server, captures every two
seconds, and uploads each JPEG with `X-Trigger: test`.

Disable `M5CAM_TEST_MODE` in `idf.py menuconfig` when validating scheduled
sleep behavior.

## Related Reference

- [Configuration](configuration.md)
- [Camera HTTP API](http-api.md)
- [Hardware power behavior](../hardware.md#power)
