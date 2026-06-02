# BM8563 Periodic Wake Fix

## Problem

Scheduled capture mode uploads one image and then stops. The firmware configures
the ESP32 internal deep-sleep timer before releasing battery hold on GPIO33.
Releasing battery hold powers off the ESP32, so its internal timer cannot start
the next boot.

## Design

Program the Timer Camera X BM8563 countdown timer before releasing battery
hold. For intervals up to four minutes, use the BM8563 `1 Hz` timer source and
write a seconds countdown. For longer intervals, use its `1/60 Hz` source and
write a minutes countdown. Clear stale timer state, enable the timer interrupt,
start the timer, and only then drive GPIO33 low. Also retain the ESP32
deep-sleep timer as a fallback for the USB-powered case, where releasing GPIO33
does not remove ESP32 power.

The firmware continues to use the existing `interval_minutes` configuration
contract. The web UI and FastAPI server need no behavioral changes.

## Error Handling

Every BM8563 I2C operation is checked. If programming the RTC timer fails, the
firmware returns the error and keeps the ESP32 powered instead of entering an
unrecoverable powered-off state.

## Verification

- Run a source-level regression check that requires BM8563 countdown setup and
  rejects the obsolete ESP32 internal timer wakeup call.
- Build the ESP-IDF firmware.
- Run the FastAPI regression suite.
- Flash hardware and confirm a new upload arrives after one minute.
