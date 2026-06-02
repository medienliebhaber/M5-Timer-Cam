import httpx
from ..config import settings


async def fetch_snapshot() -> bytes:
    """Pull a live JPEG from the camera's /snapshot endpoint."""
    # Short connect timeout so the live view doesn't stall while camera sleeps.
    # When the camera is awake it responds in <50 ms; sleeping devices cause a
    # full TCP-SYN timeout which we cap at 1 s.  The read timeout stays at 5 s
    # in case a slow WiFi link delays the JPEG transfer.
    async with httpx.AsyncClient(
        timeout=httpx.Timeout(connect=1.0, read=5.0, write=5.0, pool=5.0)
    ) as client:
        resp = await client.get(settings.camera_snapshot_url)
        resp.raise_for_status()
        return resp.content
