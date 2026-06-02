# Deferred (Buffered) Device Power-Off — Design

Date: 2026-06-03

## Problem

The web UI "Turn Off Device" button calls `POST /api/camera/power-off`, which
proxies straight to the device's `/power-off` endpoint. When the device is
asleep or otherwise offline the proxy fails with HTTP 503 and the power-off is
lost. The Timer Camera spends most of its life in deep sleep, so the common case
is exactly the one that fails.

## Goal

Buffer the power-off request as a pending device setting that is delivered the
next time the device wakes and uploads a frame, reusing the existing
frame-upload response-header config channel. Surface the pending state in the
UI and allow cancelling it before the device checks in.

## Decisions

- **Full stack**: firmware, server, and web. The device must be reflashed once
  for the buffered power-off to take effect on wake.
- **Cancellable**: the UI can cancel a pending power-off before delivery.
- **Live-first**: on trigger, attempt an immediate live power-off; if the device
  is offline, buffer it as pending instead of failing.

## Data Flow

```
[Web UI]  POST /api/camera/power-off
   |
[Server]  1. persist pending power-off (data/power_state.json)
          2. try live POST -> device /power-off (5s timeout)
             - success -> clear pending -> respond {status:"powered_off"}
             - offline -> keep pending  -> respond {status:"pending"} (HTTP 200)
   |
[Device]  next wake -> POST /api/frames
[Server]  if pending -> response header  X-Config-Power-Off: 1  -> clear pending
   |
[Firmware] sees header -> bm8563_power_off() (indefinite off) instead of
           re-arming the RTC sleep timer
```

## Server (Python) — `server/app`

### New store: `storage/power_state.py`

`PowerStateStore` backed by `data/power_state.json`:

```json
{ "power_off_pending": false, "requested_at": null }
```

Follows the existing small-JSON-store pattern (`config_store.py`,
`hardware_store.py`). Kept separate from the camera image config so the pending
flag never leaks into the device config payload returned by
`GET /api/camera/config`.

Interface:

- `get() -> dict` — returns `{"power_off_pending": bool, "requested_at": str | None}`,
  defaulting to not-pending when the file is missing or unreadable.
- `set_pending() -> None` — sets `power_off_pending = true` and `requested_at`
  to the current UTC ISO timestamp.
- `clear() -> None` — sets `power_off_pending = false`, `requested_at = null`.

### `POST /api/camera/power-off` (changed)

1. `set_pending()` (persist intent first, so a crash mid-request leaves it
   buffered).
2. Attempt live push to the device `/power-off` (5s timeout).
   - On success: `clear()`, return `{"status": "powered_off"}`.
   - On failure (offline): leave pending, return `{"status": "pending"}` with
     HTTP 200.

Offline is no longer an error; it is the buffer case.

### `DELETE /api/camera/power-off` (new)

`clear()` the pending flag. Returns `{"status": "cleared"}`. Idempotent.

### `GET /api/camera/status` (changed)

Add `power_off_pending` (and `power_off_requested_at`) to the response so the
UI's existing status fetch drives the indicator. The flag is read from
`PowerStateStore` and merged into both the `live` and `cached`/`offline`
branches.

### `POST /api/frames` (changed) — `api/frames.py`

When `power_off_pending` is true, add response header `X-Config-Power-Off: 1`
and `clear()` the flag (it has now been delivered to the device). When not
pending, omit the header.

## Firmware (C) — requires one reflash

- `resp_cfg_t` gains `char power_off[4];`
  (`firmware/components/http_client/http_client.c`).
- The `HTTP_EVENT_ON_HEADER` handler captures `X-Config-Power-Off` into it.
- `apply_resp_config` checks `power_off` **first**: if it equals `"1"`, log and
  call `bm8563_power_off()` (which releases the battery-hold GPIO and enters
  deep sleep with no wake source — never returns).

Because the directive is applied immediately after a successful upload, it works
in both firmware paths: the normal deep-sleep cycle and the USB/awake loop
(`run_awake_loop` calls `capture_and_post` each cycle). On USB power the board
enters deep sleep with no wake source, i.e. off until USB reconnect or the
hardware wake/reset button — matching the existing UI copy.

## Web UI — `web/`

- The Settings -> Device Power section gains a pending notice element:
  *"Power-off pending — applies when the device next wakes"* with a **Cancel**
  button that calls `DELETE /api/camera/power-off`.
- Trigger handler for `POST /api/camera/power-off`:
  - `{"status":"powered_off"}` -> existing success toast, close modal.
  - `{"status":"pending"}` -> info toast
    ("Device offline — power-off buffered; it will apply when the device next
    wakes") and show the pending notice.
- `loadHwStatus()` renders the pending notice (and Cancel button) whenever
  `power_off_pending` is true in the status response, and hides it otherwise.
- The in-flight request guard (`powerOffPending` JS variable) is distinct from
  the server-side buffered state; rename the in-flight guard if needed to avoid
  confusion.

## Tests — `server/tests/test_camera_api.py`

- `POST /api/camera/power-off` with device offline -> `{"status":"pending"}`,
  flag persisted.
- `POST /api/camera/power-off` with device reachable -> `{"status":"powered_off"}`,
  flag cleared.
- `DELETE /api/camera/power-off` -> clears the flag.
- `POST /api/frames` while pending -> response includes `X-Config-Power-Off: 1`
  and the flag is cleared afterward; not pending -> header absent.
- `GET /api/camera/status` includes `power_off_pending`.

## Known Trade-off

The pending flag is cleared when the header is emitted, not on device
confirmation — the device powers off and never reports back. If that single
upload response is lost in transit, the device sleeps normally and the flag is
already cleared (UI shows "not pending" while the device keeps running). This is
rare and acceptable for v1; a confirmation/retry loop is explicitly out of
scope.

## Out of Scope

- Delivery confirmation / retry of the power-off directive.
- Scheduling power-off for a future time.
- Any change to the existing wake-alarm / interval-sleep behaviour.
