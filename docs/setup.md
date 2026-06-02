# Setup

## Requirements

- M5Stack Timer Camera X and a USB-C data cable
- ESP-IDF v5.x installed and sourced
- Python 3.12+
- `uv` or `pip`
- Camera and server on the same WiFi network

## Install Server Dependencies

From the repository root:

```bash
scripts/setup.sh
```

The script installs the server package, creates `server/.env` from
`server/.env.example` when needed, and creates `data/images/`.

Set the camera network address in `server/.env`:

```dotenv
CAMERA_IP=192.168.4.1
CAMERA_PORT=80
DATA_DIR=../data
PORT=8000
```

## Configure Firmware

```bash
cd firmware
cp sdkconfig.defaults sdkconfig
idf.py menuconfig
```

Under `M5 TimerCam Configuration`, set:

- WiFi SSID
- WiFi password
- Server IP
- Server port

The checked-in defaults enable test mode for initial validation. See
[firmware configuration](firmware/configuration.md) before switching to
scheduled sleep.

## Run and Flash

```bash
# Terminal 1
cd server
uvicorn app.main:app --reload

# Terminal 2, from repository root
scripts/flash.sh
scripts/monitor.sh
```

Open [http://localhost:8000](http://localhost:8000), then run:

```bash
scripts/test_camera.sh
```

For day-to-day commands, see the [development workflow](development.md).
