# Hardware Reference — M5 Timer Camera X

## GPIO Map

### Camera (OV3660)

| Signal | GPIO |
|--------|------|
| SIOC (I2C clock) | 23 |
| SIOD (I2C data) | 25 |
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
|-----------|--------|------|
| LED | Output | 2 |
| RTC BM8563 | SCL | 14 |
| RTC BM8563 | SDA | 12 |
| Battery ADC | Analog in | 38 |
| Battery Hold | Output (keep power on) | 33 |
| HY2.0-4P connector | SCL | 13 |
| HY2.0-4P connector | SDA | 4 |

## Physical

| Property | Value |
|----------|-------|
| Size | 48 × 24 × 15 mm |
| Weight | 14g |
| Connector | USB Type-C (power + serial) |
| Grove connector | HY2.0-4P (I2C) |

## Power

| Mode | Current |
|------|---------|
| Active (WiFi + camera) | ~180mA |
| Deep sleep | ~2µA |
| Battery capacity | 140mAh |

At 5-minute intervals, active time ~3s per cycle → battery lasts approximately 2 weeks.
At 60-minute intervals → approximately 1 month.

**Battery Hold (GPIO33):** Must be driven HIGH on boot to keep the battery circuit active. Release it LOW before deep sleep to allow the RTC to cut power and achieve the 2µA sleep current.

## Camera Specs

| Spec | Value |
|------|-------|
| Sensor | OV3660 |
| Max resolution | 2048 × 1536 (3MP) |
| DFOV | 66.5° |
| Output formats | RAW 8/10-bit, RGB, YCbCr, JPEG |

## Wiring Notes

- No external wiring required for basic operation — all peripherals are onboard.
- Grove/HY2.0-4P port (GPIO13/4) is available for external I2C sensors.
- USB-C is used for flashing and serial monitoring (`idf.py flash monitor`).
