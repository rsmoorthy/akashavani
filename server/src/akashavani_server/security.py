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
    x_device_mac: Annotated[str | None, Header(alias="X-Device-Mac")] = None,
    x_firmware_version: Annotated[str | None, Header(alias="X-Firmware-Version")] = None,
    x_last_radio_at: Annotated[str | None, Header(alias="X-Last-Radio-At")] = None,
    x_uptime_seconds: Annotated[str | None, Header(alias="X-Uptime-Seconds")] = None,
    x_queue_depth: Annotated[str | None, Header(alias="X-Queue-Depth")] = None,
    x_free_heap: Annotated[str | None, Header(alias="X-Free-Heap")] = None,
    x_reset_reason: Annotated[str | None, Header(alias="X-Reset-Reason")] = None,
    x_wifi_rssi: Annotated[str | None, Header(alias="X-Wifi-Rssi")] = None,
    authorization: Annotated[str | None, Header()] = None,
    db: Session = Depends(get_db),
) -> Device:
    settings: Settings = request.app.state.settings
    device = None
    if x_device_token:
        device = db.scalar(select(Device).where(Device.token_hash == hash_token(x_device_token)))

    # The shared bearer token may be used for all device APIs. Identity then
    # comes from X-Device-Id (the MAC-derived `ak-xxxxxx`), never from a
    # station number.
    if device is None:
        supplied = _bearer_token(authorization)
        if (
            supplied is not None
            and secrets.compare_digest(supplied, settings.api_token)
            and x_device_id
        ):
            device = db.get(Device, x_device_id)

    if device is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="device token or shared bearer token with device identity required",
        )

    changed = False
    if x_device_mac:
        mac = x_device_mac.strip().lower()
        if len(mac) == 17 and mac.count(":") == 5 and device.mac_address != mac:
            device.mac_address = mac
            changed = True
    if x_firmware_version and device.firmware_version != x_firmware_version[:32]:
        device.firmware_version = x_firmware_version[:32]
        changed = True

    def _as_int(value: str | None) -> int | None:
        if value is None:
            return None
        try:
            return int(value)
        except ValueError:
            return None

    uptime_seconds = _as_int(x_uptime_seconds)
    if uptime_seconds is not None and device.uptime_seconds != uptime_seconds:
        device.uptime_seconds = uptime_seconds
        changed = True
    queue_depth = _as_int(x_queue_depth)
    if queue_depth is not None and device.queue_depth != queue_depth:
        device.queue_depth = queue_depth
        changed = True
    free_heap = _as_int(x_free_heap)
    if free_heap is not None and device.free_heap != free_heap:
        device.free_heap = free_heap
        changed = True
    wifi_rssi = _as_int(x_wifi_rssi)
    if wifi_rssi is not None and device.wifi_rssi != wifi_rssi:
        device.wifi_rssi = wifi_rssi
        changed = True
    if x_reset_reason and device.reset_reason != x_reset_reason[:16]:
        device.reset_reason = x_reset_reason[:16]
        changed = True

    if x_last_radio_at:
        try:
            radio_epoch = int(x_last_radio_at)
        except ValueError:
            radio_epoch = 0
        if radio_epoch > 0:
            radio_at = datetime.fromtimestamp(radio_epoch, UTC)
            existing_radio = device.last_radio_at
            if existing_radio is not None and existing_radio.tzinfo is None:
                existing_radio = existing_radio.replace(tzinfo=UTC)
            if existing_radio is None or existing_radio < radio_at:
                device.last_radio_at = radio_at
                changed = True
    device.last_seen_at = datetime.now(UTC)
    if changed:
        logger.info(
            "device identity id=%s mac=%s",
            device.id,
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
    """Create missing devices from AKASHAVANI_DEVICE_SEEDS.

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
