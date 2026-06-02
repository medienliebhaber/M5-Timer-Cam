# Documentation Structure Design

**Date:** 2026-06-02
**Status:** Approved

## Goal

Organize project documentation into small, focused files with one source of truth
for each fact. Keep entry-point files concise and make the documentation tree easy
to navigate for both maintainers and AI assistants.

## Principles

- `README.md` is the public landing page and points into the documentation.
- `docs/README.md` is the canonical documentation index.
- `CLAUDE.md` gives AI assistants project orientation and maintenance rules. It
  links to canonical docs instead of duplicating detailed reference material.
- Each detailed fact has one owning topic file. Index files summarize scope and
  link to the owner.
- Historical design records remain under `docs/superpowers/specs/`. They explain
  prior decisions but are not current reference documentation.
- Documentation reflects the committed product state, including camera
  management, runtime configuration, OTA updates, storage statistics, and the
  current web UI.

## Structure

```text
README.md
CLAUDE.md
docs/
  README.md
  architecture.md
  setup.md
  development.md
  hardware.md
  firmware/
    README.md
    lifecycle.md
    configuration.md
    components.md
    http-api.md
    partitions.md
  server/
    README.md
    http-api.md
    database.md
  web-ui.md
  superpowers/
    specs/
```

## Ownership Map

| Topic | Owner |
|-------|-------|
| Project summary and documentation entry point | `README.md` |
| Documentation navigation | `docs/README.md` |
| System boundaries and data flows | `docs/architecture.md` |
| First-time installation and configuration | `docs/setup.md` |
| Daily workflow, testing, and troubleshooting | `docs/development.md` |
| Board specifications, GPIO map, and power behavior | `docs/hardware.md` |
| Firmware navigation | `docs/firmware/README.md` |
| Firmware boot, awake, test, and sleep behavior | `docs/firmware/lifecycle.md` |
| Build-time and runtime firmware configuration | `docs/firmware/configuration.md` |
| Firmware component responsibilities | `docs/firmware/components.md` |
| Camera-hosted HTTP endpoints | `docs/firmware/http-api.md` |
| OTA partition table | `docs/firmware/partitions.md` |
| Server navigation, setup, environment, and storage overview | `docs/server/README.md` |
| FastAPI endpoint contract | `docs/server/http-api.md` |
| SQLite schema and image file layout | `docs/server/database.md` |
| Browser UI behavior and backend dependencies | `docs/web-ui.md` |
| AI assistant orientation and documentation maintenance rules | `CLAUDE.md` |

## Content Corrections

The rewrite must align documentation with the committed implementation:

- Use repository-root script commands consistently.
- Describe `CONFIG_M5CAM_TEST_MODE=y` as the checked-in firmware default.
- Explain that timed sleep uses the ESP32 timer and releases the battery hold;
  BM8563 provides retained clock time.
- Document camera-hosted `/snapshot`, `/status`, `/config`, and `/ota` endpoints.
- Document server camera-management, storage-statistics, and OTA proxy endpoints.
- Document the OTA-enabled partition table.
- Index the web UI reference from all relevant navigation files.

## Validation

- Check Markdown links for missing local targets.
- Search active documentation for references to removed `docs/firmware.md` and
  `docs/server.md`.
- Search for stale setup command forms and known inaccurate RTC wording.
- Review `git diff --check`.
