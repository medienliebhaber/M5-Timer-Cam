# M5 Timer Camera X

ESP-IDF firmware for the M5Stack Timer Camera X plus a FastAPI server and a
plain HTML/CSS/JavaScript browser UI.

## Start Here

- [Documentation index](docs/README.md)
- [Architecture](docs/architecture.md)
- [Setup](docs/setup.md)
- [Development workflow](docs/development.md)

## Repository Layout

| Path | Purpose |
|------|---------|
| `firmware/` | ESP-IDF camera firmware |
| `server/` | FastAPI backend and tests |
| `web/` | Static browser UI |
| `scripts/` | Setup, flash, monitor, and smoke-test helpers |
| `docs/` | Current reference documentation |
| `docs/superpowers/specs/` | Historical design records |
| `data/` | Gitignored runtime images, config cache, and SQLite database |

## Documentation Rules

- Keep each maintained fact in one canonical topic file.
- Keep `README.md`, `CLAUDE.md`, and directory `README.md` files as indexes.
- Update [docs/README.md](docs/README.md) when adding a documentation topic.
- Treat files under `docs/superpowers/specs/` as historical records, not current
  reference documentation.
- Keep secrets in gitignored `server/.env` and `firmware/sdkconfig`.

## Component References

- [Firmware](docs/firmware/README.md)
- [Server](docs/server/README.md)
- [Web UI](docs/web-ui.md)
- [Hardware](docs/hardware.md)
