from __future__ import annotations

import hashlib
import logging
import re
import secrets
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
    x_device_token: Annotated[str | None, Header(alias="X-Device-Token")] = None,
    db: Session = Depends(get_db),
) -> Device:
    if not x_device_token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="X-Device-Token required",
        )
    device = db.scalar(select(Device).where(Device.token_hash == hash_token(x_device_token)))
    if device is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="unknown device token",
        )
    return device


def require_admin(
    request: Request,
    authorization: Annotated[str | None, Header()] = None,
) -> None:
    settings: Settings = request.app.state.settings
    if not authorization or not authorization.lower().startswith("bearer "):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="admin bearer token required",
        )
    supplied = authorization[7:]
    if not secrets.compare_digest(supplied, settings.admin_token):
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
