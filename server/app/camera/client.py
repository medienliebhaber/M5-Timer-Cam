import httpx
from ..config import settings


async def fetch_snapshot() -> bytes:
    """Pull a live JPEG from the camera's /snapshot endpoint."""
    async with httpx.AsyncClient(timeout=5.0) as client:
        resp = await client.get(settings.camera_snapshot_url)
        resp.raise_for_status()
        return resp.content
