#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../firmware"

if ! command -v idf.py &>/dev/null; then
  echo "ERROR: idf.py not found. Source ESP-IDF first:"
  echo "  . \$HOME/esp/esp-idf/export.sh"
  exit 1
fi

if [ ! -f sdkconfig ]; then
  echo "ERROR: sdkconfig not found."
  echo "  cp sdkconfig.defaults sdkconfig && idf.py menuconfig"
  exit 1
fi

echo "=== Building and flashing M5 TimerCam firmware ==="
idf.py build flash "$@"
