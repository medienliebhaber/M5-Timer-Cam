# Database and Image Layout

The server stores runtime files under `DATA_DIR`, which defaults to `../data`
when the application runs from `server/`.

```text
data/
  camera.db
  camera_config.json
  images/
    YYYY/
      MM/
        DD/
          HH-MM-SS_{trigger}.jpg
```

## SQLite Schema

`camera.db` stores image metadata.

```sql
CREATE TABLE IF NOT EXISTS frames (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    captured_at TEXT    NOT NULL,
    trigger     TEXT    NOT NULL,
    filename    TEXT    NOT NULL,
    filesize    INTEGER,
    width       INTEGER,
    height      INTEGER
);

CREATE INDEX IF NOT EXISTS idx_frames_captured_at
ON frames (captured_at DESC);
```

## Runtime Config Cache

`camera_config.json` stores the latest server-side capture interval and sleep
preference. The server uses it when the camera is unavailable and sends it to
the camera on its next upload.
