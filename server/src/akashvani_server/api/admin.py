from __future__ import annotations

import hashlib
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Annotated
from uuid import uuid4

from fastapi import APIRouter, Depends, HTTPException, Query, Request, status
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Command, Device, FirmwareRelease, Observation
from ..schemas import (
    CommandCreate,
    CommandOut,
    DeviceCreate,
    DeviceOut,
    FirmwareRegister,
    FirmwareReleaseOut,
    ObservationOut,
)
from ..security import hash_token, require_admin

router = APIRouter(
    prefix="/api/v1/admin",
    tags=["admin"],
    dependencies=[Depends(require_admin)],
)


@router.post("/devices", response_model=DeviceOut)
def create_or_rotate_device(
    body: DeviceCreate,
    db: Session = Depends(get_db),
) -> Device:
    token_hash = hash_token(body.token)
    owner = db.scalar(select(Device).where(Device.token_hash == token_hash, Device.id != body.id))
    if owner is not None:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"token already belongs to device {owner.id}",
        )

    device = db.get(Device, body.id)
    if device is None:
        device = Device(id=body.id, name=body.name, token_hash=token_hash)
        db.add(device)
    else:
        device.name = body.name or device.name
        device.token_hash = token_hash
    db.commit()
    db.refresh(device)
    return device


@router.get("/devices", response_model=list[DeviceOut])
def list_devices(db: Session = Depends(get_db)) -> list[Device]:
    return list(db.scalars(select(Device).order_by(Device.id)).all())


@router.post("/devices/{device_id}/commands", response_model=CommandOut)
def enqueue_command(
    device_id: str,
    body: CommandCreate,
    request: Request,
    db: Session = Depends(get_db),
) -> Command:
    device = db.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="device not found")

    settings = request.app.state.settings
    ttl = body.expires_in_seconds or settings.command_default_ttl_seconds
    command = Command(
        id=str(uuid4()),
        device_id=device_id,
        type=body.type.value,
        payload=body.payload,
        status="pending",
        expires_at=datetime.now(UTC) + timedelta(seconds=ttl),
    )
    db.add(command)
    db.commit()
    db.refresh(command)
    return command


@router.get("/observations", response_model=list[ObservationOut])
def list_observations(
    device_id: str | None = None,
    limit: Annotated[int, Query(ge=1, le=500)] = 100,
    offset: Annotated[int, Query(ge=0)] = 0,
    db: Session = Depends(get_db),
) -> list[Observation]:
    statement = select(Observation).order_by(Observation.received_at.desc())
    if device_id is not None:
        statement = statement.where(Observation.device_id == device_id)
    statement = statement.offset(offset).limit(limit)
    return list(db.scalars(statement).all())


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


@router.post("/firmware", response_model=FirmwareReleaseOut, status_code=status.HTTP_201_CREATED)
def register_firmware(
    body: FirmwareRegister,
    request: Request,
    db: Session = Depends(get_db),
) -> FirmwareRelease:
    filename = Path(body.filename).name
    if filename != body.filename or not filename:
        raise HTTPException(
            status_code=422,
            detail="filename must be a plain file name",
        )

    firmware_dir = request.app.state.settings.firmware_dir.resolve()
    path = (firmware_dir / filename).resolve()
    if path.parent != firmware_dir or not path.is_file():
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"{filename} not found in {firmware_dir}",
        )
    if db.get(FirmwareRelease, body.version) is not None:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"version {body.version} already registered",
        )

    release = FirmwareRelease(
        version=body.version,
        channel=body.channel,
        filename=filename,
        sha256=_sha256(path),
        size_bytes=path.stat().st_size,
        notes=body.notes,
        mandatory=body.mandatory,
    )
    db.add(release)
    db.commit()
    db.refresh(release)
    return release


@router.get("/firmware", response_model=list[FirmwareReleaseOut])
def list_firmware(db: Session = Depends(get_db)) -> list[FirmwareRelease]:
    return list(
        db.scalars(select(FirmwareRelease).order_by(FirmwareRelease.created_at.desc())).all()
    )
