import pytest
from unittest.mock import AsyncMock, patch
from fastapi.testclient import TestClient
import httpx


SAMPLE_JPEG = b"\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9"


def test_snapshot_ok(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])

    with patch("app.api.snapshot.fetch_snapshot", new_callable=AsyncMock) as mock_fetch:
        mock_fetch.return_value = SAMPLE_JPEG
        from app.main import app
        client = TestClient(app)
        resp = client.get("/api/snapshot")

    assert resp.status_code == 200
    assert resp.headers["content-type"] == "image/jpeg"
    assert resp.headers["X-Snapshot-Source"] == "live"
    assert resp.content == SAMPLE_JPEG


def test_snapshot_camera_offline(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])

    with patch("app.api.snapshot.fetch_snapshot", new_callable=AsyncMock) as mock_fetch:
        mock_fetch.side_effect = httpx.RequestError("unreachable")
        from app.main import app
        client = TestClient(app)
        resp = client.get("/api/snapshot")

    assert resp.status_code == 503


def test_snapshot_camera_offline_returns_identified_cached_frame(tmp_data, monkeypatch):
    import app.config as cfg
    from app.storage.repository import FrameRepository

    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    filename = "2026/06/02/14-30-00_timer.jpg"
    path = tmp_data["images_dir"] / filename
    path.parent.mkdir(parents=True)
    path.write_bytes(SAMPLE_JPEG)
    FrameRepository(tmp_data["db_path"]).insert(
        captured_at="2026-06-02T14:30:00Z",
        trigger="timer",
        filename=filename,
    )

    with patch("app.api.snapshot.fetch_snapshot", new_callable=AsyncMock) as mock_fetch:
        mock_fetch.side_effect = httpx.RequestError("unreachable")
        from app.main import app
        client = TestClient(app)
        resp = client.get("/api/snapshot")

    assert resp.status_code == 200
    assert resp.headers["X-Snapshot-Source"] == "cached"
    assert resp.headers["X-Captured-At"] == "2026-06-02T14:30:00Z"
    assert resp.content == SAMPLE_JPEG
