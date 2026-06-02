# M5 Timer Camera X — Setup Design Spec

**Date:** 2026-06-02
**Status:** Approved

## Problem

New M5 Timer Camera X hardware with no project structure. Need a well-organized, AI-assistant-first workspace covering firmware, web server, frontend, documentation, and a camera test program.

## Decisions Made

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Firmware toolchain | ESP-IDF native | Maximum control, production-grade, full ESP32 feature access |
| Server language | Python + FastAPI | Async, auto-docs, easy to extend with ML/CV later |
| Image delivery | Both push (timer) + pull (live) | Complete coverage: scheduled history + on-demand live view |
| Image storage | SQLite + filesystem | Metadata-rich queries without ops overhead of a server DB |
| Frontend | Vanilla HTML/JS | No build step, trivially served as static files |
| CLAUDE.md | Single root file | One source of truth; component detail lives in docs/ |

## Architecture

```
[M5 TimerCam X]  ──POST /api/frames──▶  [FastAPI server]  ──▶  SQLite + filesystem
(ESP32, OV3660)  ◀──GET /snapshot────   localhost:8000          data/images/ + camera.db
                                                │
                                         [Browser UI]
                                         live tab + gallery tab
```

## Firmware Components

| Component | Responsibility |
|-----------|---------------|
| `camera` | OV3660 init + JPEG capture |
| `wifi` | STA connect + retry |
| `http_client` | POST frame to server |
| `http_server` | Serve GET /snapshot |
| `rtc` | BM8563 time + deep-sleep wake |
| `led` | Status blink patterns |

## Server API

| Method | Path | Description |
|--------|------|-------------|
| POST | /api/frames | Ingest JPEG from camera |
| GET | /api/frames | List frames (paginated + date filter) |
| GET | /api/frames/{id} | Serve JPEG file |
| GET | /api/snapshot | Live pull from camera |
| WS | /ws/live | ~1fps live stream to browser |
| GET | / | Frontend |

## SQLite Schema

```sql
CREATE TABLE frames (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    captured_at TEXT NOT NULL,
    trigger TEXT NOT NULL,
    filename TEXT NOT NULL,
    filesize INTEGER,
    width INTEGER,
    height INTEGER
);
```

## Out of Scope (Future)

- Authentication / access control
- Cloud storage (S3/compatible)
- Motion detection / computer vision
- Mobile app
- Multi-camera support
