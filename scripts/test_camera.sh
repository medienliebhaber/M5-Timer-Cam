#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Load .env
if [ -f server/.env ]; then
  set -a; source server/.env; set +a
fi

CAMERA_IP="${CAMERA_IP:-192.168.4.1}"
PORT="${PORT:-8000}"
PASS=0
FAIL=0

ok()   { echo "  [PASS] $1"; ((PASS++)) || true; }
fail() { echo "  [FAIL] $1"; ((FAIL++)) || true; }

echo ""
echo "=== M5 TimerCam smoke test ==="
echo "Camera: $CAMERA_IP | Server: localhost:$PORT"
echo ""

# 1. Ping camera
echo "1. Ping camera..."
if ping -c1 -W2 "$CAMERA_IP" &>/dev/null; then
  ok "Camera reachable at $CAMERA_IP"
else
  fail "Cannot ping $CAMERA_IP — check CAMERA_IP in server/.env and WiFi"
fi

# 2. Pull snapshot from camera
echo "2. GET /snapshot from camera..."
SNAP_FILE=$(mktemp /tmp/snap.XXXXXX.jpg)
HTTP_CODE=$(curl -s -o "$SNAP_FILE" -w "%{http_code}" \
  --max-time 5 "http://$CAMERA_IP/snapshot" 2>/dev/null || echo "000")
if [ "$HTTP_CODE" = "200" ] && [ -s "$SNAP_FILE" ]; then
  SIZE=$(wc -c < "$SNAP_FILE")
  ok "Snapshot received ($SIZE bytes)"
else
  fail "GET /snapshot failed (HTTP $HTTP_CODE)"
fi
rm -f "$SNAP_FILE"

# 3. POST test frame to server
echo "3. POST test frame to server..."
TEST_JPEG=$(python3 -c "import sys; sys.stdout.buffer.write(bytes.fromhex('ffd8ffe000104a4649460001010000010001000000ffd9'))")
TMP_JPEG=$(mktemp /tmp/test.XXXXXX.jpg)
printf '\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9' > "$TMP_JPEG"

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" \
  --max-time 5 -X POST "http://localhost:$PORT/api/frames" \
  -H "Content-Type: image/jpeg" \
  -H "X-Trigger: test" \
  --data-binary "@$TMP_JPEG" 2>/dev/null || echo "000")
rm -f "$TMP_JPEG"

if [ "$HTTP_CODE" = "201" ]; then
  ok "Frame accepted by server (HTTP 201)"
else
  fail "POST /api/frames failed (HTTP $HTTP_CODE) — is the server running?"
fi

# 4. Verify frame in API
echo "4. Verify frame in GET /api/frames..."
TOTAL=$(curl -s --max-time 5 "http://localhost:$PORT/api/frames" 2>/dev/null \
  | python3 -c "import sys,json; print(json.load(sys.stdin).get('total', 0))" 2>/dev/null || echo 0)
if [ "$TOTAL" -gt 0 ]; then
  ok "Gallery has $TOTAL frame(s)"
else
  fail "No frames found in GET /api/frames"
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
