from __future__ import annotations

from functools import lru_cache
from pathlib import Path

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """Runtime configuration. All environment variables use AKASHAVANI_."""

    model_config = SettingsConfigDict(
        env_prefix="AKASHAVANI_",
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    app_name: str = "Akashavani API"
    environment: str = "development"
    debug: bool = False
    log_level: str = "INFO"

    database_url: str = "sqlite:///./data/akashavani.db"
    data_dir: Path = Path("./data")
    firmware_dir: Path = Path("./data/firmware")

    # Comma-separated `device-id:token:display-name` seeds, applied at startup.
    # Only missing devices are created; existing tokens are never overwritten.
    device_seeds: str = (
        "sim-001:dev-sim-token:Simulator," "station-1:dev-station-1-token:Bench station 1"
    )
    admin_token: str = "dev-admin-token"
    # Shared bearer token used by the archive API and accepted by the admin API.
    # The receiver's hotspot page stores the same value.
    api_token: str = "secret"

    # Site metadata used by the server-side derived calculations. Location and
    # altitude are per device (reported with its configuration), not global.
    rain_bucket_mm: float = Field(default=0.254, gt=0.0)

    max_body_bytes: int = Field(default=64 * 1024, ge=1024)
    max_firmware_bytes: int = Field(default=8 * 1024 * 1024, ge=1024)
    max_future_skew_seconds: int = Field(default=300, ge=0)
    command_default_ttl_seconds: int = Field(default=3600, ge=60)
    # A delivered command that is not acknowledged in this window is treated as
    # lost (reboot/crash) and probed/retried, up to three deliveries.
    command_ack_timeout_seconds: int = Field(default=300, ge=30)
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
