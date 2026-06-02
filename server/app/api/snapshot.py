import httpx
from fastapi import APIRouter, Depends
from fastapi.responses import FileResponse, Response

from ..camera.client import fetch_snapshot
from ..config import settings
from ..storage.repository import FrameRepository

router = APIRouter(tags=["snapshot"])


def get_repo() -> FrameRepository:
    return FrameRepository(settings.db_path)


@router.get("/api/snapshot")
async def live_snapshot(repo: FrameRepository = Depends(get_repo)):
    try:
        data = await fetch_snapshot()
        return Response(content=data, media_type="image/jpeg",
                        headers={"Cache-Control": "no-store"})
    except (httpx.RequestError, httpx.HTTPStatusError):
        pass

    frame = repo.get_latest()
    if frame is None:
        return Response(status_code=503, content="camera offline")
    path = settings.images_dir / frame.filename
    if not path.exists():
        return Response(status_code=503, content="camera offline")
    return FileResponse(str(path), media_type="image/jpeg",
                        headers={"Cache-Control": "no-store"})
