from pydantic_settings import BaseSettings
from pathlib import Path


class Settings(BaseSettings):
    camera_ip: str = "192.168.4.1"
    camera_port: int = 80
    data_dir: Path = Path("../data")
    port: int = 8000

    model_config = {"env_file": ".env", "env_file_encoding": "utf-8"}

    @property
    def images_dir(self) -> Path:
        return self.data_dir / "images"

    @property
    def db_path(self) -> Path:
        return self.data_dir / "camera.db"

    @property
    def camera_snapshot_url(self) -> str:
        return f"http://{self.camera_ip}:{self.camera_port}/snapshot"


settings = Settings()
