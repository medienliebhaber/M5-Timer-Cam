#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../firmware"

# Source ESP-IDF if not already active
if ! command -v idf.py &>/dev/null; then
  export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.3_py3.12_env"
  . "$HOME/esp/esp-idf/export.sh" > /dev/null 2>&1
fi

if ! command -v idf.py &>/dev/null; then
  echo "ERROR: idf.py not found. Run: . ~/esp/esp-idf/export.sh"
  exit 1
fi

if [ ! -f sdkconfig ]; then
  echo "ERROR: sdkconfig not found."
  echo "  cp sdkconfig.defaults sdkconfig && idf.py menuconfig"
  exit 1
fi

echo "=== Building and flashing M5 TimerCam firmware ==="
idf.py build flash "$@"
