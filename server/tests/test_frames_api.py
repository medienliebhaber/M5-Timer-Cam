import pytest
from pathlib import Path
from fastapi.testclient import TestClient


SAMPLE_JPEG = (
    b"\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"
    b"\xff\xd9"  # minimal valid JPEG
)


def test_ingest_frame(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    from app.main import app
    client = TestClient(app)

    resp = client.post(
        "/api/frames",
        content=SAMPLE_JPEG,
        headers={
            "Content-Type": "image/jpeg",
            "X-Captured-At": "2026-06-02T14:30:00Z",
            "X-Trigger": "timer",
        },
    )
    assert resp.status_code == 201
    data = resp.json()
    assert "id" in data
    assert "filename" in data


def test_list_frames_empty(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    from app.main import app
    client = TestClient(app)

    resp = client.get("/api/frames")
    assert resp.status_code == 200
    body = resp.json()
    assert body["total"] == 0
    assert body["frames"] == []


def test_list_frames_after_ingest(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    from app.main import app
    client = TestClient(app)

    client.post(
        "/api/frames",
        content=SAMPLE_JPEG,
        headers={"Content-Type": "image/jpeg", "X-Trigger": "test"},
    )
    resp = client.get("/api/frames")
    assert resp.json()["total"] == 1


def test_get_frame_not_found(tmp_data, monkeypatch):
    import app.config as cfg
    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    from app.main import app
    client = TestClient(app)

    resp = client.get("/api/frames/9999")
    assert resp.status_code == 404
