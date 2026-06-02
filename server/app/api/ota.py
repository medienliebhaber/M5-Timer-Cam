import httpx
from fastapi import APIRouter, HTTPException, Request

from ..config import settings

router = APIRouter(tags=["ota"])


@router.post("/api/ota")
async def push_ota(request: Request):
    """Receive a firmware .bin and forward it to the camera's /ota endpoint."""
    body = await request.body()
    if not body:
        raise HTTPException(400, "empty firmware binary")

    try:
        async with httpx.AsyncClient(timeout=120.0) as client:
            r = await client.post(
                f"http://{settings.camera_ip}:{settings.camera_port}/ota",
                content=body,
                headers={"Content-Type": "application/octet-stream"},
            )
            r.raise_for_status()
            return {"status": "ok", "message": r.text}
    except httpx.RequestError as exc:
        raise HTTPException(503, f"camera unreachable: {exc}")
    except httpx.HTTPStatusError as exc:
        raise HTTPException(502, f"camera OTA failed: {exc.response.text}")
