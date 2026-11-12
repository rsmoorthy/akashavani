from __future__ import annotations

import hashlib
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Annotated
from uuid import uuid4

from fastapi import (
    APIRouter,
    Depends,
    File,
    Form,
    HTTPException,
    Query,
    Request,
    UploadFile,
    status,
)
from pydantic import ValidationError
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import ArchiveRecord, Command, Device, FirmwareRelease, Observation
from ..schemas import (
    ArchiveOut,
    CommandCreate,
    CommandOut,
    CommandType,
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
    payload = dict(body.payload)

    if body.type == CommandType.DOWNLOAD_NEW_FW:
        version = payload.get("version")
        if version:
            release = db.get(FirmwareRelease, str(version))
        else:
            release = db.scalar(
                select(FirmwareRelease)
                .where(FirmwareRelease.channel == "stable")
                .order_by(FirmwareRelease.created_at.desc())
                .limit(1)
            )
        if release is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="no matching firmware release",
            )
        payload.update(
            {
                "version": release.version,
                "url": f"/api/v1/firmware/{release.version}/download",
                "sha256": release.sha256,
                "size_bytes": release.size_bytes,
            }
        )
    elif body.type == CommandType.BOOT_WITH_NEW_FW:
        version = payload.get("version")
        if version and db.get(FirmwareRelease, str(version)) is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"unknown firmware version {version}",
            )

    command = Command(
        id=str(uuid4()),
        device_id=device_id,
        type=body.type.value,
        payload=payload,
        status="pending",
        expires_at=datetime.now(UTC) + timedelta(seconds=ttl),
    )
    db.add(command)
    db.commit()
    db.refresh(command)
    return command


@router.get("/devices/{device_id}/commands", response_model=list[CommandOut])
def list_commands(
    device_id: str,
    limit: Annotated[int, Query(ge=1, le=200)] = 50,
    db: Session = Depends(get_db),
) -> list[Command]:
    if db.get(Device, device_id) is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="device not found")
    return list(
        db.scalars(
            select(Command)
            .where(Command.device_id == device_id)
            .order_by(Command.created_at.desc())
            .limit(limit)
        ).all()
    )


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


@router.get("/archive", response_model=list[ArchiveOut])
def list_archive(
    device_id: str | None = None,
    limit: Annotated[int, Query(ge=1, le=500)] = 100,
    offset: Annotated[int, Query(ge=0)] = 0,
    db: Session = Depends(get_db),
) -> list[ArchiveRecord]:
    statement = select(ArchiveRecord).order_by(ArchiveRecord.date_time.desc())
    if device_id is not None:
        statement = statement.where(ArchiveRecord.device_id == device_id)
    statement = statement.offset(offset).limit(limit)
    return list(db.scalars(statement).all())


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


@router.post(
    "/firmware/upload",
    response_model=FirmwareReleaseOut,
    status_code=status.HTTP_201_CREATED,
)
async def upload_firmware(
    request: Request,
    file: Annotated[UploadFile, File()],
    version: Annotated[str, Form()],
    channel: Annotated[str, Form()] = "stable",
    notes: Annotated[str | None, Form()] = None,
    mandatory: Annotated[bool, Form()] = False,
    db: Session = Depends(get_db),
) -> FirmwareRelease:
    original = Path(file.filename or "").name
    if not original:
        original = f"akashvani-{version}.bin"
    filename = f"akashvani-{version}-{original}"
    try:
        meta = FirmwareRegister(
            version=version,
            channel=channel,
            filename=filename,
            notes=notes,
            mandatory=mandatory,
        )
    except ValidationError as exc:
        raise HTTPException(
            status_code=422,
            detail=exc.errors(include_url=False),
        ) from exc
    if db.get(FirmwareRelease, meta.version) is not None:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"version {meta.version} already registered",
        )

    settings = request.app.state.settings
    firmware_dir = settings.firmware_dir.resolve()
    firmware_dir.mkdir(parents=True, exist_ok=True)
    path = (firmware_dir / meta.filename).resolve()
    if path.parent != firmware_dir:
        raise HTTPException(status_code=422, detail="invalid filename")

    digest = hashlib.sha256()
    size = 0
    try:
        with path.open("wb") as output:
            while chunk := await file.read(1024 * 1024):
                size += len(chunk)
                if size > settings.max_firmware_bytes:
                    raise HTTPException(
                        status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
                        detail=f"firmware exceeds {settings.max_firmware_bytes} bytes",
                    )
                digest.update(chunk)
                output.write(chunk)
        if size == 0:
            raise HTTPException(status_code=422, detail="empty firmware file")
    except Exception:
        path.unlink(missing_ok=True)
        raise
    finally:
        await file.close()

    release = FirmwareRelease(
        version=meta.version,
        channel=meta.channel,
        filename=meta.filename,
        sha256=digest.hexdigest(),
        size_bytes=size,
        notes=meta.notes,
        mandatory=meta.mandatory,
    )
    db.add(release)
    db.commit()
    db.refresh(release)
    return release


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
