# Web UI Reference

The UI is served at `http://localhost:8000` (or whatever host the server runs on).
No build step — plain HTML/CSS/JS, assets under `web/`.

Backend endpoint contracts live in the [server HTTP API](server/http-api.md).

---

## Layout

### Header
- **Logo** — M5 TimerCam
- **Tabs** — Live / Gallery / Playback (desktop only; hidden on mobile)
- **Status badge** — Online (green pulse) / Offline (red); reflects last `/api/snapshot` result
- **⚙ Settings button** — opens the Settings modal

### Bottom navigation (mobile only)
Three tab buttons fixed to the bottom of the viewport: Live, Gallery, Playback.

---

## Tabs

### Live
Polls `GET /api/snapshot` every second and displays the latest JPEG.
When the camera is sleeping, falls back to the most recently stored frame and
shows an amber **Cached frame** badge with its stored capture timestamp.
Fresh device snapshots show a green **Live** badge.

### Gallery
Paginated grid of stored frames (30 per page), newest first.

| Control | Behaviour |
|---------|-----------|
| **From / To** datetime pickers | Filter results to a time range |
| **Filter** button | Apply the current date range |
| **Clear** button | Remove filters, reload all |
| **Refresh** button | Reload the current page |
| Frame count badge | Total frames matching the current filter |
| Prev / Next | Paginate |

Clicking a thumbnail opens the **Lightbox**:
- Prev / Next arrows (or ← → keys) navigate within the current page
- Download button saves the full JPEG
- Esc or backdrop click closes it

### Playback
Step through a sequence of stored frames like a timelapse.

| Control | Behaviour |
|---------|-----------|
| **From / To** pickers | Select the time window to load |
| **Preset buttons** | 10 min / 1 h / 24 h / 7 d / 30 d — fills pickers relative to newest frame |
| **Available range hint** | Shows oldest → newest frame timestamps in the database |
| **Speed slider** | 1–300 fps |
| **Load** | Fetches up to 6 000 frames for the selected range, oldest-first |
| **Play / Pause / Stop** | Playback controls |
| Progress bar | Scrubs through the loaded sequence |
| Frame counter + timestamp | Current position in the sequence |

Toasts appear for load errors, empty results, and successful loads.

---

## Settings modal

Opened with the ⚙ gear icon. Four sections:

### Camera Hardware
Live hardware telemetry fetched from `GET /api/camera/status`.
Shows "Camera offline" when the camera is sleeping.

| Field | Description |
|-------|-------------|
| Battery | Percentage bar + % value (from ADC on GPIO38) |
| Free RAM | Camera free heap in KB |
| WiFi | RSSI in dBm |

**Refresh** button re-fetches immediately.

### Camera Config
Controls pulled from `GET /api/camera/config` (live if camera is online, server-cached otherwise).

| Control | Description |
|---------|-------------|
| Capture interval slider | 1–60 minutes between frames |
| Deep sleep toggle | Checked = sleep between captures; unchecked = stay awake continuously |
| Preview image | Fresh JPEG captured by the real camera after image-control changes |
| Resolution | VGA through QXGA |
| JPEG quality | 0–63; lower values produce higher quality |
| Brightness / contrast / saturation / sharpness | OV3660 image tuning sliders |
| Horizontal mirror / vertical flip | Device-side orientation controls |

**Save Config** POSTs to `POST /api/camera/config`.
- If the camera is online the config is applied immediately.
- If offline, it is stored server-side and delivered via `X-Config-*` response headers the next time the camera posts a frame.

Image changes are debounced and sent to `POST /api/camera/preview`. Preview
changes alter the real OV3660 registers but are not durable until **Save
Config**. Closing the modal without saving restores the last saved image
settings when the camera is reachable.

> **Sleep & USB:** When the deep sleep toggle is off, or when the camera detects USB charging (battery voltage > 4.1 V), it runs an awake loop — WiFi and HTTP server stay up, frames are captured at the configured interval without entering deep sleep.

### Device Power
**Turn Off Device** asks for confirmation, then requests an immediate shutdown.
Scheduled captures stop until USB is unplugged and reconnected or a hardware
wake/reset occurs. The request fails when the camera is offline.

### Server Storage
Fetched from `GET /api/storage` on modal open.

| Field | Description |
|-------|-------------|
| Frames | Total frames in the SQLite database |
| Disk used | Sum of all JPEG files in `data/images/` |

---

## Notifications (toasts)

Temporary banners appear at the bottom of the screen:

| Type | Colour | Example triggers |
|------|--------|-----------------|
| success | green | Frames loaded, config saved & applied |
| warn | amber | No frames found, config queued for next wake |
| error | red | Network error, server error |
