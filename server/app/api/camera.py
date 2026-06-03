from typing import Any, Literal

import httpx
from fastapi import APIRouter, HTTPException
from fastapi.responses import Response
from pydantic import BaseModel, ConfigDict, Field

from ..config import settings
from ..storage.config_store import CameraConfigStore
from ..storage.hardware_store import HardwareStateStore
from ..storage.power_state import PowerStateStore
from ..storage.repository import FrameRepository

router = APIRouter(tags=["camera"])

FrameSize = Literal["VGA", "SVGA", "XGA", "SXGA", "UXGA", "QXGA"]


class ImageConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    framesize: FrameSize
    quality: int = Field(ge=0, le=63)
    brightness: int = Field(ge=-3, le=3)
    contrast: int = Field(ge=-3, le=3)
    saturation: int = Field(ge=-4, le=4)
    sharpness: int = Field(ge=-3, le=3)
    hmirror: bool
    vflip: bool


class CameraConfigUpdate(BaseModel):
    model_config = ConfigDict(extra="forbid")

    interval_minutes: int | None = Field(default=None, ge=1, le=1440)
    sleep_enabled: bool | None = None
    framesize: FrameSize | None = None
    quality: int | None = Field(default=None, ge=0, le=63)
    brightness: int | None = Field(default=None, ge=-3, le=3)
    contrast: int | None = Field(default=None, ge=-3, le=3)
    saturation: int | None = Field(default=None, ge=-4, le=4)
    sharpness: int | None = Field(default=None, ge=-3, le=3)
    hmirror: bool | None = None
    vflip: bool | None = None


def _camera_url(path: str) -> str:
    return f"http://{settings.camera_ip}:{settings.camera_port}{path}"


def _store() -> CameraConfigStore:
    return CameraConfigStore(settings.data_dir)


@router.get("/api/camera/status")
async def camera_status() -> Any:
    hw = HardwareStateStore(settings.data_dir)
    power_store = PowerStateStore(settings.data_dir)
    power_state = power_store.get()
    pending_fields = {
        "power_off_pending": power_state["power_off_pending"],
        "power_off_requested_at": power_state["requested_at"],
    }
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.get(_camera_url("/status"))
            r.raise_for_status()
            data = r.json()
        hw.save(data)
        return {**data, **pending_fields, "source": "live"}
    except Exception:
        cached = hw.get()
        if cached:
            return {**cached, **pending_fields, "source": "cached"}
        return {"source": "offline", **pending_fields}


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
async def set_camera_config(update: CameraConfigUpdate) -> Any:
    body = update.model_dump(exclude_none=True)
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


@router.post("/api/camera/preview")
async def preview_camera_config(config: ImageConfig) -> Response:
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            r = await client.post(
                _camera_url("/preview"),
                json=config.model_dump(),
                headers={"Content-Type": "application/json"},
            )
            r.raise_for_status()
            return Response(
                content=r.content,
                media_type="image/jpeg",
                headers={"Cache-Control": "no-store"},
            )
    except Exception as exc:
        raise HTTPException(503, f"camera offline: {exc}")


@router.post("/api/camera/power-off")
async def power_off_camera() -> Any:
    power_store = PowerStateStore(settings.data_dir)
    power_store.set_pending()
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.post(_camera_url("/power-off"))
            r.raise_for_status()
        power_store.clear()
        return {"status": "powered_off"}
    except Exception:
        return {"status": "pending"}


@router.delete("/api/camera/power-off")
async def cancel_power_off_camera() -> Any:
    power_store = PowerStateStore(settings.data_dir)
    power_store.clear()
    return {"status": "cleared"}


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
