from unittest.mock import AsyncMock, patch

import httpx


IMAGE_DEFAULTS = {
    "framesize": "UXGA",
    "quality": 12,
    "brightness": 0,
    "contrast": 0,
    "saturation": 0,
    "sharpness": 0,
    "hmirror": True,
    "vflip": True,
}


def _response(status_code=200, json_data=None, content=b""):
    request = httpx.Request("POST", "http://camera")
    return httpx.Response(
        status_code,
        json=json_data,
        content=content if json_data is None else None,
        request=request,
    )


def test_get_camera_config_returns_image_defaults_when_camera_offline(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        async_client.return_value.__aenter__.return_value.get = AsyncMock(
            side_effect=httpx.ConnectError("offline")
        )

        response = client.get("/api/camera/config")

    assert response.status_code == 200
    assert response.json() == {
        "interval_minutes": 1,
        "sleep_enabled": True,
        **IMAGE_DEFAULTS,
        "source": "cached",
    }


def test_post_camera_config_merges_partial_image_settings(client):
    from app.config import settings
    from app.storage.config_store import CameraConfigStore

    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        async_client.return_value.__aenter__.return_value.post = AsyncMock(
            return_value=_response(json_data={"status": "ok"})
        )

        response = client.post(
            "/api/camera/config",
            json={"contrast": 2, "quality": 8},
        )

    assert response.status_code == 200
    assert response.json() == {"status": "saved", "pushed_to_camera": True}
    cached = CameraConfigStore(settings.data_dir).get()
    assert cached["contrast"] == 2
    assert cached["quality"] == 8


def test_post_camera_config_rejects_out_of_range_image_setting(client):
    response = client.post("/api/camera/config", json={"contrast": 4})

    assert response.status_code == 422


def test_post_camera_config_queues_settings_when_camera_offline(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        async_client.return_value.__aenter__.return_value.post = AsyncMock(
            side_effect=httpx.ConnectError("offline")
        )

        response = client.post("/api/camera/config", json={"sharpness": -2})

    assert response.status_code == 200
    assert response.json() == {"status": "saved", "pushed_to_camera": False}


def test_post_camera_preview_returns_live_device_jpeg(client):
    jpeg = b"\xff\xd8preview\xff\xd9"
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        async_client.return_value.__aenter__.return_value.post = AsyncMock(
            return_value=_response(content=jpeg)
        )

        response = client.post("/api/camera/preview", json=IMAGE_DEFAULTS)

    assert response.status_code == 200
    assert response.headers["content-type"] == "image/jpeg"
    assert response.content == jpeg


def test_post_camera_preview_returns_503_when_camera_offline(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        async_client.return_value.__aenter__.return_value.post = AsyncMock(
            side_effect=httpx.ConnectError("offline")
        )

        response = client.post("/api/camera/preview", json=IMAGE_DEFAULTS)

    assert response.status_code == 503


def test_post_camera_power_off_forwards_to_live_device(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        post = async_client.return_value.__aenter__.return_value.post = AsyncMock(
            return_value=_response(json_data={"status": "powering_off"})
        )

        response = client.post("/api/camera/power-off")

    assert response.status_code == 200
    assert response.json() == {"status": "powering_off"}
    post.assert_awaited_once()
    assert post.await_args.args[0].endswith("/power-off")


def test_post_camera_power_off_returns_503_when_camera_offline(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        post = async_client.return_value.__aenter__.return_value.post = AsyncMock(
            side_effect=httpx.ConnectError("offline")
        )

        response = client.post("/api/camera/power-off")

    assert response.status_code == 503
    post.assert_awaited_once()


def test_post_camera_power_off_returns_503_when_camera_rejects_request(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        post = async_client.return_value.__aenter__.return_value.post = AsyncMock(
            return_value=_response(status_code=409)
        )

        response = client.post("/api/camera/power-off")

    assert response.status_code == 503
    post.assert_awaited_once()
