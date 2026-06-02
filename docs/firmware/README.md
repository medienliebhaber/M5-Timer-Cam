# Firmware Reference

ESP-IDF firmware under `firmware/` runs on the M5Stack Timer Camera X.

## Guides

| Guide | Purpose |
|-------|---------|
| [Lifecycle](lifecycle.md) | Boot, capture, awake, test, and sleep behavior |
| [Configuration](configuration.md) | Build-time settings and runtime camera configuration |
| [Components](components.md) | Firmware module responsibilities |
| [Camera HTTP API](http-api.md) | Endpoints exposed by the camera while awake |
| [Partitions](partitions.md) | Flash layout for OTA updates |

## Common Commands

From the repository root:

```bash
scripts/flash.sh
scripts/monitor.sh
```

Run `idf.py menuconfig` from `firmware/` to edit device configuration.
