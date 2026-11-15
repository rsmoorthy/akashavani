from __future__ import annotations

from .models import Device, utcnow
from .schemas import DeviceConfig, DeviceConfigOut


def store_device_config(device: Device, config: DeviceConfig) -> int:
    """Replace the server mirror with ``config`` and refresh extracted columns."""
    version = (device.config_version or 0) + 1
    device.config = config.model_dump(mode="json")
    device.config_version = version
    device.config_updated_at = utcnow()
    device.timezone = config.timezone
    if config.site is not None:
        device.latitude = config.site.latitude
        device.longitude = config.site.longitude
        device.altitude_m = config.site.altitude_m
    else:
        device.latitude = None
        device.longitude = None
        device.altitude_m = None
    return version


def device_config_out(device: Device) -> DeviceConfigOut:
    config: DeviceConfig | None = None
    if device.config:
        config = DeviceConfig.model_validate(device.config)
    return DeviceConfigOut(
        device_id=device.id,
        config_version=device.config_version,
        config_updated_at=device.config_updated_at,
        config=config,
    )
