import httpx
from fastapi import APIRouter
from fastapi.responses import Response

from ..camera.client import fetch_snapshot

router = APIRouter(tags=["snapshot"])


@router.get("/api/snapshot")
async def live_snapshot():
    try:
        data = await fetch_snapshot()
    except (httpx.RequestError, httpx.HTTPStatusError):
        return Response(status_code=503, content="camera unreachable")
    return Response(content=data, media_type="image/jpeg",
                    headers={"Cache-Control": "no-store"})
