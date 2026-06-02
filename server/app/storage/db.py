import sqlite3
from pathlib import Path


def init_db(db_path: Path) -> None:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS frames (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            captured_at TEXT    NOT NULL,
            trigger     TEXT    NOT NULL,
            filename    TEXT    NOT NULL,
            filesize    INTEGER,
            width       INTEGER,
            height      INTEGER
        )
    """)
    conn.execute("""
        CREATE INDEX IF NOT EXISTS idx_frames_captured_at
        ON frames (captured_at DESC)
    """)
    conn.commit()
    conn.close()


def get_conn(db_path: Path) -> sqlite3.Connection:
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn
