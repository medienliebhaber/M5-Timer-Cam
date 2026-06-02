# Hardware Reference

M5Stack Timer Camera X board details used by the firmware.

## Camera

| Property | Value |
|----------|-------|
| Sensor | OV3660 |
| Maximum resolution | 2048 x 1536 |
| Default firmware resolution | UXGA, 1600 x 1200 |
| DFOV | 66.5 degrees |

## GPIO Map

### OV3660

| Signal | GPIO |
|--------|------|
| SIOC | 23 |
| SIOD | 25 |
| XCLK | 27 |
| VSYNC | 22 |
| HREF | 26 |
| PCLK | 21 |
| D0 | 32 |
| D1 | 35 |
| D2 | 34 |
| D3 | 5 |
| D4 | 39 |
| D5 | 18 |
| D6 | 36 |
| D7 | 19 |
| RESET | 15 |

### Peripherals

| Peripheral | Signal | GPIO |
|------------|--------|------|
| Status LED | Output | 2 |
| BM8563 | SCL | 14 |
| BM8563 | SDA | 12 |
| Battery ADC | Analog input | 38 |
| Battery hold | Output | 33 |
| HY2.0-4P connector | SCL | 13 |
| HY2.0-4P connector | SDA | 4 |

## Power

| Property | Value |
|----------|-------|
| Battery | 140 mAh |
| Active current | Approximately 180 mA with WiFi and camera active |
| Low-power current | Approximately 2 uA |

The firmware drives battery hold on GPIO33 high during startup. Before timed
sleep it configures the ESP32 timer wakeup and releases GPIO33 low. The BM8563
retains wall-clock time so captured images can be timestamped after wake.

When battery voltage exceeds 4.1 V, the firmware assumes USB charging and stays
awake.

## Physical

| Property | Value |
|----------|-------|
| Size | 48 x 24 x 15 mm |
| Weight | 14 g |
| USB | Type-C for power, flashing, and serial monitoring |
| Expansion | HY2.0-4P I2C connector |

## Related Reference

- [Firmware lifecycle](firmware/lifecycle.md)
- [Firmware components](firmware/components.md)
