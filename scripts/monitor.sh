#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../firmware"

if ! command -v idf.py &>/dev/null; then
  echo "ERROR: idf.py not found. Source ESP-IDF first."
  exit 1
fi

echo "=== Attaching serial monitor (Ctrl+] to exit) ==="
idf.py monitor "$@"
