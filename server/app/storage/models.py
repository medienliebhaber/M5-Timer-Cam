from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class Frame:
    id: int
    captured_at: str      # ISO8601 UTC
    trigger: str          # "timer" | "test" | "manual"
    filename: str         # relative path under DATA_DIR/images/
    filesize: Optional[int]
    width: Optional[int]
    height: Optional[int]

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "captured_at": self.captured_at,
            "trigger": self.trigger,
            "filename": self.filename,
            "filesize": self.filesize,
            "width": self.width,
            "height": self.height,
        }
