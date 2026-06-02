import json
from pathlib import Path

_DEFAULT: dict = {"interval_minutes": 1, "sleep_enabled": True}


class CameraConfigStore:
    def __init__(self, data_dir: Path):
        self._path = data_dir / "camera_config.json"

    def get(self) -> dict:
        try:
            return {**_DEFAULT, **json.loads(self._path.read_text())}
        except Exception:
            return dict(_DEFAULT)

    def save(self, updates: dict) -> None:
        merged = {**self.get(), **updates}
        self._path.write_text(json.dumps(merged))
