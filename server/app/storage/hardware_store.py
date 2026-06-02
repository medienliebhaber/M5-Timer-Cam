import json
from datetime import datetime, timezone
from pathlib import Path


class HardwareStateStore:
    def __init__(self, data_dir: Path):
        self._path = data_dir / "hardware_state.json"

    def save(self, data: dict) -> None:
        record = {**data, "seen_at": datetime.now(timezone.utc).isoformat()}
        self._path.write_text(json.dumps(record))

    def get(self) -> dict | None:
        try:
            return json.loads(self._path.read_text())
        except Exception:
            return None
