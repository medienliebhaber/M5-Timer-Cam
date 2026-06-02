#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../firmware"

if ! command -v idf.py &>/dev/null; then
  export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.3_py3.12_env"
  . "$HOME/esp/esp-idf/export.sh" > /dev/null 2>&1
fi

echo "=== Attaching serial monitor (Ctrl+] to exit) ==="
idf.py monitor "$@"
