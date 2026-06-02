from typing import Any

import httpx
from fastapi import APIRouter, HTTPException, Request

from ..config import settings
from ..storage.config_store import CameraConfigStore
from ..storage.repository import FrameRepository

router = APIRouter(tags=["camera"])

_CAMERA_BASE = f"http://{{ip}}:{{port}}"


def _camera_url(path: str) -> str:
    return f"http://{settings.camera_ip}:{settings.camera_port}{path}"


def _store() -> CameraConfigStore:
    return CameraConfigStore(settings.data_dir)


@router.get("/api/camera/status")
async def camera_status() -> Any:
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.get(_camera_url("/status"))
            r.raise_for_status()
            return r.json()
    except Exception as exc:
        raise HTTPException(503, f"camera offline: {exc}")


@router.get("/api/camera/config")
async def get_camera_config() -> Any:
    stored = _store().get()
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.get(_camera_url("/config"))
            r.raise_for_status()
            return {**stored, **r.json(), "source": "live"}
    except Exception:
        return {**stored, "source": "cached"}


@router.post("/api/camera/config")
async def set_camera_config(request: Request) -> Any:
    body: dict = await request.json()
    _store().save(body)

    pushed = False
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.post(
                _camera_url("/config"),
                json=body,
                headers={"Content-Type": "application/json"},
            )
            r.raise_for_status()
            pushed = True
    except Exception:
        pass

    return {"status": "saved", "pushed_to_camera": pushed}


@router.get("/api/storage")
async def storage_stats() -> Any:
    repo = FrameRepository(settings.db_path)
    frame_count = repo.count_frames()

    total_bytes = 0
    if settings.images_dir.exists():
        for f in settings.images_dir.rglob("*.jpg"):
            total_bytes += f.stat().st_size

    return {
        "frame_count": frame_count,
        "disk_used_bytes": total_bytes,
        "disk_used_mb": round(total_bytes / 1024 / 1024, 1),
    }
