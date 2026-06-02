#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

echo "=== M5 TimerCam setup ==="

# Python deps
cd server
if command -v uv &>/dev/null; then
  uv pip install -e ".[dev]"
else
  pip install -e ".[dev]"
fi

# .env
if [ ! -f .env ]; then
  cp .env.example .env
  echo "Created server/.env — edit CAMERA_IP if needed"
fi

cd ..

# data dirs
mkdir -p data/images
echo "data/ directory ready"

echo ""
echo "Setup complete. Next steps:"
echo "  1. Edit server/.env (set CAMERA_IP)"
echo "  2. cd server && uvicorn app.main:app --reload"
echo "  3. Open http://localhost:8000"
