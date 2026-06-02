import pytest
import tempfile
from pathlib import Path
from fastapi.testclient import TestClient

from app.storage.db import init_db
from app.storage.repository import FrameRepository


@pytest.fixture
def tmp_data(tmp_path):
    db_path = tmp_path / "camera.db"
    images_dir = tmp_path / "images"
    images_dir.mkdir()
    init_db(db_path)
    return {"db_path": db_path, "images_dir": images_dir, "data_dir": tmp_path}


@pytest.fixture
def repo(tmp_data):
    return FrameRepository(tmp_data["db_path"])


@pytest.fixture
def client(tmp_data, monkeypatch):
    from app import config as cfg_module
    monkeypatch.setattr(cfg_module.settings, "data_dir", tmp_data["data_dir"])

    from app.main import app
    return TestClient(app)
