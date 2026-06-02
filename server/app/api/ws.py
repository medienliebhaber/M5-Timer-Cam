import asyncio
import httpx
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from ..camera.client import fetch_snapshot

router = APIRouter(tags=["websocket"])

FRAME_INTERVAL = 1.0  # seconds between frames


@router.websocket("/ws/live")
async def live_ws(websocket: WebSocket):
    await websocket.accept()
    try:
        while True:
            try:
                data = await fetch_snapshot()
                await websocket.send_bytes(data)
            except (httpx.RequestError, httpx.HTTPStatusError):
                # Camera offline — send empty ping to keep connection alive
                await websocket.send_text("offline")
            await asyncio.sleep(FRAME_INTERVAL)
    except WebSocketDisconnect:
        pass
