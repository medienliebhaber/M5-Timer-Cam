# Calibrated Battery Voltage Readings

## Problem

The firmware reads the Timer Camera X battery voltage in both `main.c` and the
HTTP server. Each path takes one raw ADC sample and applies a fixed linear
conversion. The duplicated approximation is more susceptible to noise and does
not use the ESP32 ADC calibration support used by the official M5TimerCAM
Arduino library.

## Design

Add a shared `battery` firmware component for GPIO38 / `ADC1_CHANNEL_2`.
Initialize the ADC once during application startup and characterize it with
`esp_adc_cal`. Follow the official M5TimerCAM Arduino implementation when
measuring voltage: average 64 raw ADC samples, convert the average with
`esp_adc_cal_raw_to_voltage`, and apply the TimerCAM board scale of `0.661`.

Expose a small C API:

- `battery_init()` configures the ADC channel and calibration data.
- `battery_get_voltage_mv()` returns the calibrated, averaged battery voltage.

Use the shared helper from both existing consumers:

- `main.c` keeps its existing USB-connected heuristic: voltage greater than
  `4100 mV`.
- The HTTP `/status` endpoint keeps returning `battery_mv` and keeps its
  existing percentage mapping from `3300..4200 mV`, clamped to `0..100`.

Battery percentage behavior is intentionally unchanged. This work improves the
voltage input only.

## Error Handling

`battery_init()` returns an ESP-IDF error if calibration storage cannot be
allocated. Application startup checks the result before any battery reading.
The legacy ADC configuration calls do not report errors. The voltage getter
assumes successful initialization and returns an integer millivolt value.

## Testing

Keep averaging and scaled millivolt calculation behind a testable helper so
source-level or host-side regression coverage can validate the calculation
without ADC hardware. Verify that both firmware consumers use the shared
component and that the old duplicated raw ADC conversion is removed.

Build the ESP-IDF firmware after implementation. On hardware, request `/status`
repeatedly and confirm that `battery_mv` readings are stable and plausible.
Confirm that the unchanged percentage mapping and `4100 mV` USB threshold still
receive the calibrated voltage value.

## Documentation

Update the maintained firmware component reference to list the new `battery`
component. Historical design records remain under `docs/superpowers/specs/`.
