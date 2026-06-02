from pathlib import Path
from typing import Optional
from .db import get_conn
from .models import Frame


class FrameRepository:
    def __init__(self, db_path: Path):
        self._db_path = db_path

    def insert(
        self,
        captured_at: str,
        trigger: str,
        filename: str,
        filesize: Optional[int] = None,
        width: Optional[int] = None,
        height: Optional[int] = None,
    ) -> Frame:
        with get_conn(self._db_path) as conn:
            cur = conn.execute(
                """INSERT INTO frames
                   (captured_at, trigger, filename, filesize, width, height)
                   VALUES (?, ?, ?, ?, ?, ?)""",
                (captured_at, trigger, filename, filesize, width, height),
            )
            conn.commit()
            return self.get_by_id(cur.lastrowid)

    def get_by_id(self, frame_id: int) -> Optional[Frame]:
        with get_conn(self._db_path) as conn:
            row = conn.execute(
                "SELECT * FROM frames WHERE id = ?", (frame_id,)
            ).fetchone()
        if row is None:
            return None
        return Frame(**dict(row))

    def list_frames(
        self,
        page: int = 1,
        per_page: int = 50,
        from_dt: Optional[str] = None,
        to_dt: Optional[str] = None,
    ) -> tuple[list[Frame], int]:
        conditions = []
        params: list = []

        if from_dt:
            conditions.append("captured_at >= ?")
            params.append(from_dt)
        if to_dt:
            conditions.append("captured_at <= ?")
            params.append(to_dt)

        where = ("WHERE " + " AND ".join(conditions)) if conditions else ""
        offset = (page - 1) * per_page

        with get_conn(self._db_path) as conn:
            total = conn.execute(
                f"SELECT COUNT(*) FROM frames {where}", params
            ).fetchone()[0]

            rows = conn.execute(
                f"SELECT * FROM frames {where} ORDER BY captured_at DESC "
                f"LIMIT ? OFFSET ?",
                params + [per_page, offset],
            ).fetchall()

        frames = [Frame(**dict(row)) for row in rows]
        return frames, total
