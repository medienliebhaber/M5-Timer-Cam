from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles

from .config import settings
from .storage.db import init_db
from .api import frames, snapshot, ws


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings.images_dir.mkdir(parents=True, exist_ok=True)
    init_db(settings.db_path)
    yield


app = FastAPI(title="M5 TimerCam Server", lifespan=lifespan)

app.include_router(frames.router)
app.include_router(snapshot.router)
app.include_router(ws.router)

# Serve frontend at /
web_dir = Path(__file__).parent.parent.parent / "web"
if web_dir.exists():
    app.mount("/", StaticFiles(directory=web_dir, html=True), name="static")
