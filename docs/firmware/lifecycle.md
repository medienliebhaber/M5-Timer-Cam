# Firmware Lifecycle

## Boot

On boot the firmware initializes NVS, the BM8563 clock interface, the camera,
and the status LED.

## Scheduled Sleep Mode

When test mode is disabled and runtime sleep is enabled:

1. Connect to WiFi.
2. Capture a JPEG and post it to the server.
3. Apply runtime configuration returned in `X-Config-*` response headers.
4. Start the camera HTTP server for five seconds.
5. Configure the ESP32 timer wakeup.
6. Release battery hold on GPIO33 and enter deep sleep.

The BM8563 retains wall-clock time for capture timestamps. Timed wakeup is
configured through the ESP32 deep-sleep timer.

## Awake Mode

The camera stays awake when runtime sleep is disabled or USB charging is
inferred from battery voltage above 4.1 V. It keeps WiFi and the camera HTTP
server available and captures at the configured interval.

If sleep is re-enabled after USB is disconnected, the firmware restarts and
returns to the scheduled sleep flow.

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
