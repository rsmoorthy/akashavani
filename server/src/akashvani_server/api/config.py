from __future__ import annotations

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from ..db import get_db
from ..device_config import store_device_config
from ..models import Device
from ..schemas import DeviceConfig, DeviceConfigAccepted
from ..security import require_device

router = APIRouter(prefix="/api/v1/devices/me", tags=["config"])


@router.put("/config", response_model=DeviceConfigAccepted)
def report_config(
    body: DeviceConfig,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> DeviceConfigAccepted:
    """Store the configuration document reported by the device."""
    version = store_device_config(device, body)
    db.commit()
    return DeviceConfigAccepted(config_version=version)
