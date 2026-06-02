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


def test_ingest_frame_replaces_implausible_camera_timestamp(tmp_data, monkeypatch):
    import app.config as cfg

    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    from app.main import app
    client = TestClient(app)

    response = client.post(
        "/api/frames",
        content=SAMPLE_JPEG,
        headers={"X-Captured-At": "2000-04-21T04:51:17Z"},
    )
    frame = client.get("/api/frames").json()["frames"][0]

    assert response.status_code == 201
    assert frame["captured_at"] != "2000-04-21T04:51:17Z"
    assert frame["filename"].startswith("2026/06/02/")


def test_ingest_frame_returns_queued_image_config_headers(tmp_data, monkeypatch):
    import app.config as cfg
    from app.storage.config_store import CameraConfigStore

    monkeypatch.setattr(cfg.settings, "data_dir", tmp_data["data_dir"])
    CameraConfigStore(tmp_data["data_dir"]).save(
        {
            "interval_minutes": 7,
            "sleep_enabled": False,
            "framesize": "QXGA",
            "quality": 9,
            "brightness": -1,
            "contrast": 2,
            "saturation": 3,
            "sharpness": -2,
            "hmirror": False,
            "vflip": True,
        }
    )
    from app.main import app
    client = TestClient(app)

    response = client.post("/api/frames", content=SAMPLE_JPEG)

    assert response.headers["X-Config-Interval"] == "7"
    assert response.headers["X-Config-Sleep"] == "0"
    assert response.headers["X-Config-Framesize"] == "QXGA"
    assert response.headers["X-Config-Quality"] == "9"
    assert response.headers["X-Config-Brightness"] == "-1"
    assert response.headers["X-Config-Contrast"] == "2"
    assert response.headers["X-Config-Saturation"] == "3"
    assert response.headers["X-Config-Sharpness"] == "-2"
    assert response.headers["X-Config-Hmirror"] == "0"
    assert response.headers["X-Config-Vflip"] == "1"


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
