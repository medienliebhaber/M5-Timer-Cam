import pytest
from unittest.mock import AsyncMock, patch
from fastapi.testclient import TestClient
import httpx


SAMPLE_JPEG = b"\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9"


def test_snapshot_ok(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])

    with patch("app.camera.client.fetch_snapshot", new_callable=AsyncMock) as mock_fetch:
        mock_fetch.return_value = SAMPLE_JPEG
        from app.main import app
        client = TestClient(app)
        resp = client.get("/api/snapshot")

    assert resp.status_code == 200
    assert resp.headers["content-type"] == "image/jpeg"
    assert resp.content == SAMPLE_JPEG


def test_snapshot_camera_offline(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])

    with patch("app.camera.client.fetch_snapshot", new_callable=AsyncMock) as mock_fetch:
        mock_fetch.side_effect = httpx.RequestError("unreachable")
        from app.main import app
        client = TestClient(app)
        resp = client.get("/api/snapshot")

    assert resp.status_code == 503
