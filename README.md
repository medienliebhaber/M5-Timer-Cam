# M5 Timer Camera X

M5Stack Timer Camera X firmware and a local FastAPI application for scheduled
JPEG capture, live preview, gallery browsing, and timelapse playback.

## Features

- Scheduled camera capture with optional deep sleep
- Browser live view with fallback to the latest stored frame
- Gallery filtering and timelapse playback
- Runtime capture interval and sleep configuration
- Camera health reporting and OTA firmware updates

## Quick Start

```bash
git clone https://github.com/maskow/M5-Timer-Cam
cd M5-Timer-Cam

scripts/setup.sh

# Terminal 1
cd server
uvicorn app.main:app --reload

# Terminal 2
cd firmware
cp sdkconfig.defaults sdkconfig
idf.py menuconfig
cd ..
scripts/flash.sh
scripts/monitor.sh
```

Open [http://localhost:8000](http://localhost:8000).

## Documentation

Start with the [documentation index](docs/README.md).

- [Architecture](docs/architecture.md)
- [Setup](docs/setup.md)
- [Development workflow](docs/development.md)
- [Hardware reference](docs/hardware.md)
- [Firmware reference](docs/firmware/README.md)
- [Server reference](docs/server/README.md)
- [Web UI reference](docs/web-ui.md)
