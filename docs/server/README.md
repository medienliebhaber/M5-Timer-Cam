# Server Reference

The FastAPI application under `server/` accepts camera uploads, stores JPEG
metadata, proxies camera management requests, and serves the browser UI.

## Setup

From the repository root:

```bash
scripts/setup.sh
```

Run the development server:

```bash
cd server
uvicorn app.main:app --reload
```

Open [http://localhost:8000](http://localhost:8000). FastAPI's generated API
documentation is available at [http://localhost:8000/docs](http://localhost:8000/docs).

## Environment

`server/.env` is created from `server/.env.example` by `scripts/setup.sh`.

| Variable | Default | Description |
|----------|---------|-------------|
| `CAMERA_IP` | `192.168.4.1` | Camera address on the local network |
| `CAMERA_PORT` | `80` | Camera HTTP server port |
| `DATA_DIR` | `../data` | Root for images, SQLite, and cached runtime config |
| `PORT` | `8000` | Server listen port used by helper scripts |

## Storage

The server stores JPEG files, SQLite metadata, and the last server-side camera
configuration under `DATA_DIR`.

See [database and image layout](database.md).

## References

- [Server HTTP API](http-api.md)
- [Database and image layout](database.md)
- [Camera-hosted HTTP API](../firmware/http-api.md)
