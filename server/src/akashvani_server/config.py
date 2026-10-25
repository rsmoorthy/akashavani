from __future__ import annotations

from functools import lru_cache
from pathlib import Path

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """Runtime configuration. All environment variables use AKASHVANI_."""

    model_config = SettingsConfigDict(
        env_prefix="AKASHVANI_",
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    app_name: str = "Akashvani API"
    environment: str = "development"
    debug: bool = False
    log_level: str = "INFO"

    database_url: str = "sqlite:///./data/akashvani.db"
    data_dir: Path = Path("./data")
    firmware_dir: Path = Path("./data/firmware")

    # Comma-separated `device-id:token:display-name` seeds, applied at startup.
    # Only missing devices are created; existing tokens are never overwritten.
    device_seeds: str = "sim-001:dev-sim-token:Simulator"
    admin_token: str = "dev-admin-token"

    max_body_bytes: int = Field(default=64 * 1024, ge=1024)
    max_future_skew_seconds: int = Field(default=300, ge=0)
    command_default_ttl_seconds: int = Field(default=3600, ge=60)
    cors_origins: str = ""

    @property
    def firmware_path(self) -> Path:
        return self.firmware_dir

    @property
    def cors_origin_list(self) -> list[str]:
        return [item.strip() for item in self.cors_origins.split(",") if item.strip()]


@lru_cache
def get_settings() -> Settings:
    return Settings()
