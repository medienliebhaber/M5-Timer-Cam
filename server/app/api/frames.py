from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import aiofiles
from fastapi import APIRouter, Depends, Header, HTTPException, Request, Response
from fastapi.responses import FileResponse

from ..config import settings
from ..storage.repository import FrameRepository

router = APIRouter(prefix="/api/frames", tags=["frames"])


def get_repo() -> FrameRepository:
    return FrameRepository(settings.db_path)


@router.post("", status_code=201)
async def ingest_frame(
    request: Request,
    x_captured_at: Optional[str] = Header(None),
    x_trigger: Optional[str] = Header(None),
    repo: FrameRepository = Depends(get_repo),
):
    body = await request.body()
    if not body:
        raise HTTPException(400, "empty body")

    captured_at = x_captured_at or datetime.now(timezone.utc).isoformat()
    trigger = x_trigger or "manual"

    # Determine image dimensions via Pillow (best-effort)
    width = height = None
    try:
        from io import BytesIO
        from PIL import Image
        img = Image.open(BytesIO(body))
        width, height = img.size
    except Exception:
        pass

    # Build storage path: YYYY/MM/DD/HH-MM-SS_{trigger}.jpg
    try:
        dt = datetime.fromisoformat(captured_at.replace("Z", "+00:00"))
    except ValueError:
        dt = datetime.now(timezone.utc)

    rel = dt.strftime(f"%Y/%m/%d/%H-%M-%S_{trigger}.jpg")
    abs_path = settings.images_dir / rel
    abs_path.parent.mkdir(parents=True, exist_ok=True)

    async with aiofiles.open(abs_path, "wb") as f:
        await f.write(body)

    frame = repo.insert(
        captured_at=captured_at,
        trigger=trigger,
        filename=rel,
        filesize=len(body),
        width=width,
        height=height,
    )
    return {"id": frame.id, "filename": rel}


@router.get("")
def list_frames(
    page: int = 1,
    per_page: int = 50,
    from_dt: Optional[str] = None,
    to_dt: Optional[str] = None,
    repo: FrameRepository = Depends(get_repo),
):
    frames, total = repo.list_frames(page, per_page, from_dt, to_dt)
    return {
        "total": total,
        "page": page,
        "per_page": per_page,
        "frames": [f.to_dict() for f in frames],
    }


@router.get("/{frame_id}")
def get_frame(frame_id: int, repo: FrameRepository = Depends(get_repo)):
    frame = repo.get_by_id(frame_id)
    if frame is None:
        raise HTTPException(404, "frame not found")
    path = settings.images_dir / frame.filename
    if not path.exists():
        raise HTTPException(404, "image file not found")
    return FileResponse(path, media_type="image/jpeg")
