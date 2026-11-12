from __future__ import annotations

import hashlib
import logging
import re
import secrets
from datetime import UTC, datetime
from typing import Annotated

from fastapi import Depends, Header, HTTPException, Request, status
from sqlalchemy import Engine, select
from sqlalchemy.orm import Session

from .config import Settings
from .db import get_db
from .models import Device

logger = logging.getLogger(__name__)
DEVICE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]{2,31}$")


def hash_token(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def require_device(
    request: Request,
    x_device_token: Annotated[str | None, Header(alias="X-Device-Token")] = None,
    x_device_id: Annotated[str | None, Header(alias="X-Device-Id")] = None,
    x_station_id: Annotated[str | None, Header(alias="X-Station-Id")] = None,
    x_device_mac: Annotated[str | None, Header(alias="X-Device-Mac")] = None,
    x_firmware_version: Annotated[str | None, Header(alias="X-Firmware-Version")] = None,
    authorization: Annotated[str | None, Header()] = None,
    db: Session = Depends(get_db),
) -> Device:
    settings: Settings = request.app.state.settings
    device = None
    if x_device_token:
        device = db.scalar(select(Device).where(Device.token_hash == hash_token(x_device_token)))

    # The shared bearer token may be used for all device APIs. Identity comes
    # from X-Device-Id or the station ID in that case.
    if device is None:
        supplied = _bearer_token(authorization)
        if supplied is not None and secrets.compare_digest(supplied, settings.api_token):
            if x_device_id:
                device = db.get(Device, x_device_id)
            elif x_station_id:
                try:
                    station_id = int(x_station_id)
                except ValueError:
                    station_id = 0
                if station_id > 0:
                    device = db.scalar(select(Device).where(Device.station_id == station_id))

    if device is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="device token or shared bearer token with device identity required",
        )

    changed = False
    if x_station_id:
        header_station_id: int | None
        try:
            header_station_id = int(x_station_id)
        except ValueError:
            header_station_id = None
        if (
            header_station_id is not None
            and 0 < header_station_id <= 65535
            and device.station_id != header_station_id
        ):
            device.station_id = header_station_id
            changed = True
    if x_device_mac:
        mac = x_device_mac.strip().lower()
        if len(mac) == 17 and mac.count(":") == 5 and device.mac_address != mac:
            device.mac_address = mac
            changed = True
    if x_firmware_version and device.firmware_version != x_firmware_version[:32]:
        device.firmware_version = x_firmware_version[:32]
        changed = True
    device.last_seen_at = datetime.now(UTC)
    if changed:
        logger.info(
            "device identity id=%s station=%s mac=%s",
            device.id,
            device.station_id,
            device.mac_address,
        )
    db.commit()
    return device


def _bearer_token(authorization: str | None) -> str | None:
    if not authorization or not authorization.lower().startswith("bearer "):
        return None
    return authorization[7:]


def require_api_token(
    request: Request,
    authorization: Annotated[str | None, Header()] = None,
) -> None:
    """Authenticate the shared archive/API bearer token."""
    settings: Settings = request.app.state.settings
    supplied = _bearer_token(authorization)
    if supplied is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="bearer token required",
        )
    if not secrets.compare_digest(supplied, settings.api_token):
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="invalid bearer token")


def require_admin(
    request: Request,
    authorization: Annotated[str | None, Header()] = None,
) -> None:
    settings: Settings = request.app.state.settings
    supplied = _bearer_token(authorization)
    if supplied is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="admin bearer token required",
        )
    if not (
        secrets.compare_digest(supplied, settings.admin_token)
        or secrets.compare_digest(supplied, settings.api_token)
    ):
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="invalid admin token")


def parse_device_seeds(value: str) -> list[tuple[str, str, str]]:
    seeds: list[tuple[str, str, str]] = []
    for item in value.split(","):
        item = item.strip()
        if not item:
            continue
        fields = item.split(":", 2)
        if len(fields) < 2:
            logger.warning("ignoring malformed device seed (expected id:token[:name])")
            continue
        device_id, token = fields[0].strip(), fields[1].strip()
        name = fields[2].strip() if len(fields) > 2 else device_id
        if not DEVICE_ID_RE.match(device_id):
            logger.warning("ignoring device seed with invalid id %r", device_id)
            continue
        if len(token) < 8:
            logger.warning("ignoring device seed %s with too-short token", device_id)
            continue
        seeds.append((device_id, token, name))
    return seeds


def seed_devices(engine: Engine, settings: Settings) -> None:
    """Create missing devices from AKASHVANI_DEVICE_SEEDS.

    Existing rows are never overwritten: rotating a token is an explicit admin
    action, not a side effect of a restart.
    """
    from sqlalchemy.orm import Session as SessionType

    with SessionType(engine) as db:
        for device_id, token, name in parse_device_seeds(settings.device_seeds):
            existing = db.get(Device, device_id)
            if existing is not None:
                continue
            token_hash = hash_token(token)
            owner = db.scalar(select(Device).where(Device.token_hash == token_hash))
            if owner is not None:
                logger.warning(
                    "device seed %s skipped: token already belongs to %s", device_id, owner.id
                )
                continue
            db.add(Device(id=device_id, name=name, token_hash=token_hash))
            logger.info("seeded device %s", device_id)
        db.commit()
