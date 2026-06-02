# Web UI Device Power Off

## Summary

Add an explicit web UI action that turns off the Timer Camera X indefinitely.
Unlike scheduled sleep, manual power off must not arm the BM8563 countdown
timer. The device remains inactive until USB power is unplugged and
reconnected or the hardware wake/reset button is pressed.

## Goals

- Add a guarded **Turn Off Device** action to the browser settings modal.
- Proxy the action through the FastAPI server to the camera firmware.
- Release battery hold on GPIO33 without programming a scheduled wake.
- Enter ESP32 deep sleep without a timer wake source so USB-powered devices
  also remain inactive.
- Preserve all saved capture and image configuration.

## Non-Goals

- Do not add a scheduled-sleep button.
- Do not queue a shutdown request while the camera is offline.
- Do not change the existing scheduled BM8563 wake flow.
- Do not add a remote power-on action. A powered-off device is unreachable.

## Firmware

Add `POST /power-off` to the camera HTTP server.

The handler sends a successful JSON response first, waits briefly so the HTTP
response can leave the device, then invokes a new RTC component helper for
indefinite shutdown. The helper:

1. Disables any stale BM8563 countdown timer state.
2. Releases battery hold by driving GPIO33 low.
3. Enters ESP32 deep sleep without enabling a timer wake source.

Disabling stale BM8563 timer state prevents an earlier scheduled timer from
unexpectedly waking a manually powered-off device. Entering timerless ESP32
deep sleep keeps a USB-powered ESP32 inactive after GPIO33 is released.

The existing `bm8563_set_wake_alarm()` scheduled-sleep path remains unchanged.

## Server API

Add `POST /api/camera/power-off`.

The FastAPI handler forwards the request to camera `POST /power-off`. It
returns the camera success response when reachable and returns `503` when the
camera is offline or rejects the request. The server does not persist or retry
the action.

## Web UI

Add a **Device Power** section to the Settings modal with:

- A short explanation that shutdown stops scheduled captures.
- A destructive **Turn Off Device** button.

Clicking the button opens a browser confirmation dialog. If confirmed, the UI
disables the button while sending `POST /api/camera/power-off`. On success it
closes the settings modal and shows a toast explaining that USB reconnect or
the hardware wake/reset button is required to start the device again. On
failure it keeps the modal open, restores the button, and shows an error toast.

## Error Handling

- Camera offline: FastAPI returns `503`; the UI reports that shutdown failed.
- BM8563 timer-disable failure: firmware returns an error and keeps the device
  powered so the user can retry instead of entering an uncertain state.
- HTTP response delivery: firmware delays briefly after responding before
  shutting down.

## Testing

- Add a server API test proving that the proxy forwards `POST /power-off`.
- Add a server API test proving that an unreachable camera returns `503`.
- Add a source-level firmware regression check requiring timer disable before
  GPIO33 release and timerless ESP32 deep sleep.
- Build the firmware and run the server test suite.
- Run a JavaScript syntax check and inspect the final diff.

## Documentation

Update the maintained firmware HTTP API, server HTTP API, web UI reference,
hardware power reference, and firmware lifecycle reference to distinguish
manual indefinite shutdown from scheduled sleep.
