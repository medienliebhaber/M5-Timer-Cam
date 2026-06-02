# Firmware Partitions

`firmware/partitions.csv` defines the 4 MB flash layout.

| Name | Type | Subtype | Offset | Size | Purpose |
|------|------|---------|--------|------|---------|
| `nvs` | `data` | `nvs` | `0x9000` | `0x5000` | Runtime settings |
| `otadata` | `data` | `ota` | `0xe000` | `0x2000` | Active OTA slot metadata |
| `phy_init` | `data` | `phy` | `0x10000` | `0x1000` | Radio initialization data |
| `ota_0` | `app` | `ota_0` | `0x20000` | `0x180000` | Application slot |
| `ota_1` | `app` | `ota_1` | `0x1A0000` | `0x180000` | Application slot |

OTA updates are written to the inactive application slot by `POST /ota`.
