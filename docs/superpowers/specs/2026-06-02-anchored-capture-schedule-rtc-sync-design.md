# Anchored Capture Schedule and RTC Synchronization

## Summary

Replace the minute-only recording interval with a one-second-resolution schedule
anchored to a configurable local time of day. Synchronize the TimerCam BM8563
RTC from the FastAPI server so the device retains the correct UTC date and time
across sleep cycles.

## Goals

- Allow capture intervals from `00:00:01` through `24:00:00`.
- Allow a daily local anchor time such as `08:00:00`.
- Align captures to slots derived from that anchor instead of drifting from the
  preceding capture.
- Use `Europe/Vienna` local wall-clock time for schedule alignment.
- Store and upload timestamps in UTC.
- Set the BM8563 complete calendar date and time from the local FastAPI server.
- Continue operating when the FastAPI time endpoint is temporarily unavailable.

## Public Configuration

Operational camera configuration uses:

```json
{
  "interval_seconds": 21600,
  "anchor_time": "08:00:00",
  "sleep_enabled": true
}
```

| Field | Values | Description |
|---|---|---|
| `interval_seconds` | `1..86400` | Capture interval in seconds |
| `anchor_time` | `HH:MM:SS` | Daily schedule anchor in `Europe/Vienna` local time |
| `sleep_enabled` | boolean | Sleep between capture slots when possible |

The image-control schema remains unchanged.

The existing `interval_minutes` field is replaced rather than retained as a
second source of truth. Existing NVS installations migrate their `interval`
minute value to seconds when `interval_sec` does not exist. Existing server
configuration files migrate `interval_minutes` to `interval_seconds` when read.

## Scheduling Semantics

The firmware treats `anchor_time` as a daily `Europe/Vienna` wall-clock anchor.
Each schedule window starts at one local day's anchor and ends immediately
before the next local day's anchor. Every window starts a new capture sequence:

```text
daily anchor + n * interval_seconds
```

where `n` is a non-negative integer and slots stop before the next window.
For example, anchor `08:00:00` and interval `06:00:00` produce `08:00`,
`14:00`, `20:00`, and `02:00`, then reset to `08:00`.

An interval does not need to divide evenly into 24 hours. Anchor `08:00:00`
and interval `07:00:00` produce `08:00`, `15:00`, `22:00`, and `05:00`, then
reset to `08:00` on the next local day. This makes the daily anchor meaningful
and prevents long-term drift.

After a capture completes, the firmware calculates the first slot strictly
after the current time. This avoids immediate duplicate captures when
processing finishes close to a slot boundary. Awake mode delays until that
slot. Sleep mode configures the ESP32 deep-sleep timer for the duration until
that slot.

Schedule calculations use `Europe/Vienna` local time. The firmware configures
the C runtime timezone with the POSIX Central European timezone rule so DST is
handled locally while the BM8563 continues storing UTC.

During DST transitions:

- Missing spring-forward local times advance to the next real aligned slot.
- Repeated fall-back local times are treated as distinct UTC instants and may
  both be capture slots when the configured sequence reaches them.

## RTC Synchronization

FastAPI exposes:

```text
GET /api/time
```

Example response:

```json
{
  "utc": "2026-06-02T14:30:42Z"
}
```

The firmware requests this endpoint after WiFi connects and before its first
capture. It parses the UTC timestamp, writes the complete calendar date and
time to the BM8563, and logs the synchronized value.

The RTC component gains:

- A BM8563 write helper for complete UTC calendar date and time.
- Read validation for the BM8563 voltage-low flag and calendar ranges.
- A helper to calculate the next anchored slot from UTC now, the local anchor,
  and the interval.
- A deep-sleep helper accepting seconds rather than minutes.

If FastAPI time synchronization fails but the BM8563 value is valid, firmware
uses retained RTC time. If both are unavailable, firmware falls back to a plain
interval delay. Uploaded timestamps remain subject to the server's existing
implausible-timestamp normalization.

## Firmware Configuration

NVS keys under `m5cam` become:

| Setting | NVS key | Description |
|---|---|---|
| Capture interval | `interval_sec` | Seconds between aligned capture slots |
| Schedule anchor | `anchor` | Local `HH:MM:SS` anchor |
| Deep sleep | `sleep_en` | Existing sleep toggle |

Build-time default configuration changes from
`M5CAM_CAPTURE_INTERVAL_MINUTES` to `M5CAM_CAPTURE_INTERVAL_SECONDS`.
The default remains five minutes (`300` seconds). The default anchor is
`00:00:00`.

Device `GET /config` and `POST /config` use `interval_seconds` and
`anchor_time`. The queued upload response headers become:

```text
X-Config-Interval-Seconds
X-Config-Anchor-Time
```

The existing image and sleep headers remain unchanged.

## Server

`CameraConfigStore` defaults and migration logic use:

```json
{
  "interval_seconds": 60,
  "anchor_time": "00:00:00"
}
```

`POST /api/camera/config` validates partial updates:

- `interval_seconds`: integer `1..86400`
- `anchor_time`: strict `HH:MM:SS`

`POST /api/frames` sends the queued schedule headers so offline device settings
take effect after the next uploaded frame.

`GET /api/time` returns server UTC and does not depend on the workstation's
display timezone.

## Web UI

Replace the minute slider with:

- Capture interval input displayed as `hh:mm:ss`, constrained to
  `00:00:01..24:00:00`.
- Daily anchor input displayed as `HH:MM:SS`.
- A short label stating that the anchor uses `Europe/Vienna` local time.

Save Config sends `interval_seconds`, `anchor_time`, sleep, and image settings
together. Offline saves remain queued by FastAPI.

## Gallery Deletion

Stored JPEG deletion applies to the FastAPI server gallery. The TimerCam does
not retain captured JPEGs after upload.

FastAPI adds:

```text
DELETE /api/frames/{id}
DELETE /api/frames?from_dt=<inclusive>&to_dt=<inclusive>
DELETE /api/frames
```

`DELETE /api/frames/{id}` removes one frame row and its JPEG file.

`DELETE /api/frames` removes every matching frame row and JPEG file. With no
query parameters it clears the full gallery. With `from_dt`, `to_dt`, or both,
it uses the same inclusive timestamp bounds as `GET /api/frames`.

All delete responses report the number of deleted rows:

```json
{
  "deleted": 42
}
```

Deletion is best-effort for missing JPEG files: the server still removes the
database row. File deletion is constrained to paths beneath `data/images`.
Empty date directories are removed after successful deletes.

The Gallery UI adds:

- A delete action in the lightbox for the currently open image.
- A **Delete filtered** action beside the gallery filters. It applies the
  current From/To query, not only the visible page.
- A **Delete all** action for the entire gallery.

Safety confirmations are mandatory:

- Individual image deletion requires a standard confirmation dialog.
- Filtered timeframe deletion requires confirmation showing the matching image
  count.
- Full-gallery deletion requires entering the exact phrase `DELETE ALL`.

After deletion, gallery results, pagination, playback state, storage stats, and
cached snapshot behavior refresh so the UI cannot continue showing removed
images.

## Validation and Testing

### Server

- Default config exposes seconds and anchor.
- Existing `interval_minutes` storage migrates to seconds.
- Partial schedule settings merge correctly.
- Intervals below one second and above 24 hours fail validation.
- Invalid anchor strings fail validation.
- `/api/frames` emits queued interval-seconds and anchor headers.
- `/api/time` emits a parseable UTC timestamp.
- Individual delete removes one frame row and JPEG.
- Individual delete returns `404` for an unknown frame.
- Filtered delete applies inclusive `from_dt` and `to_dt` bounds.
- Full-gallery delete removes every row and JPEG.
- Delete tolerates an already-missing JPEG file.
- Delete rejects or ignores any stored filename that escapes `data/images`.

### Firmware

- Build succeeds after NVS, header, and scheduling changes.
- Hardware smoke test confirms `/config` reads and writes second-resolution
  intervals and anchor values.
- Serial logs confirm FastAPI RTC synchronization writes the current UTC date.
- A short anchored interval such as `00:00:05` produces aligned uploads.
- A reboot retains correct BM8563 time and schedule settings.

### Web UI

- Interval and anchor inputs load the live or cached values.
- Save Config posts second-resolution settings.
- Offline edits remain available for queueing.
- Existing real sensor preview remains unchanged.
- Individual, filtered, and full-gallery deletion require the specified
  confirmations.
- Gallery, playback, storage stats, and cached live fallback refresh after
  deletion.

## Documentation

Update firmware, server, and web UI documentation for:

- `interval_seconds`
- `anchor_time`
- `Europe/Vienna` scheduling semantics
- `/api/time`
- BM8563 UTC synchronization and fallback behavior
- Gallery deletion endpoints and UI safety confirmations
