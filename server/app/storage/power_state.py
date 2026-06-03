import json
from datetime import datetime, timezone
from pathlib import Path


class PowerStateStore:
    def __init__(self, data_dir: Path):
        self._path = data_dir / "power_state.json"

    def get(self) -> dict:
        try:
            data = json.loads(self._path.read_text())
            return {
                "power_off_pending": bool(data.get("power_off_pending", False)),
                "requested_at": data.get("requested_at"),
            }
        except Exception:
            return {"power_off_pending": False, "requested_at": None}

    def set_pending(self) -> None:
        record = {
            "power_off_pending": True,
            "requested_at": datetime.now(timezone.utc).isoformat(),
        }
        self._path.write_text(json.dumps(record))

    def clear(self) -> None:
        record = {
            "power_off_pending": False,
            "requested_at": None,
        }
        self._path.write_text(json.dumps(record))
