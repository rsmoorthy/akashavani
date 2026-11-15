from __future__ import annotations

import csv
import hashlib
import io
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
from fastapi.responses import PlainTextResponse
from pydantic import ValidationError
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..device_config import device_config_out, store_device_config
from ..models import ArchiveRecord, Command, Device, FirmwareRelease, Observation
from ..schemas import (
    ArchiveOut,
    CommandCreate,
    CommandOut,
    CommandType,
    DeviceAdminOut,
    DeviceConfig,
    DeviceConfigOut,
    DeviceCreate,
    DeviceOut,
    FirmwareRegister,
    FirmwareReleaseOut,
    ObservationOut,
)
from ..security import hash_token, require_admin
from ..timezones import DEFAULT_DEVICE_TIMEZONE, format_in_zone, format_utc
from .raw import raw_csv_path

# A device counts as online when seen within three 30-second poll intervals.
ONLINE_WINDOW_SECONDS = 90

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
        device = Device(
            id=body.id,
            name=body.name,
            token_hash=token_hash,
            timezone=body.timezone or DEFAULT_DEVICE_TIMEZONE,
        )
        db.add(device)
    else:
        device.name = body.name or device.name
        device.token_hash = token_hash
        if body.timezone is not None:
            device.timezone = body.timezone
    db.commit()
    db.refresh(device)
    return device


@router.get("/devices", response_model=list[DeviceAdminOut])
def list_devices(db: Session = Depends(get_db)) -> list[DeviceAdminOut]:
    now = datetime.now(UTC)
    devices = db.scalars(select(Device).order_by(Device.id)).all()
    result: list[DeviceAdminOut] = []
    for device in devices:
        latest = db.scalar(
            select(ArchiveRecord)
            .where(ArchiveRecord.device_id == device.id)
            .order_by(ArchiveRecord.date_time.desc())
            .limit(1)
        )
        seen_at = device.last_seen_at
        if seen_at is not None and seen_at.tzinfo is None:
            # SQLite returns naive datetimes; rows are stored in UTC.
            seen_at = seen_at.replace(tzinfo=UTC)
        online = seen_at is not None and (now - seen_at).total_seconds() <= ONLINE_WINDOW_SECONDS
        result.append(
            DeviceAdminOut(
                id=device.id,
                name=device.name,
                timezone=device.timezone,
                latitude=device.latitude,
                longitude=device.longitude,
                altitude_m=device.altitude_m,
                mac_address=device.mac_address,
                firmware_version=device.firmware_version,
                created_at=device.created_at,
                last_seen_at=device.last_seen_at,
                last_radio_at=device.last_radio_at,
                uptime_seconds=device.uptime_seconds,
                queue_depth=device.queue_depth,
                free_heap=device.free_heap,
                reset_reason=device.reset_reason,
                wifi_rssi=device.wifi_rssi,
                online=online,
                latest_archive_at=latest.date_time if latest else None,
                frames_valid=latest.frames_valid if latest else None,
                frames_expected=latest.frames_expected if latest else None,
                frames_crc_bad=latest.frames_crc_bad if latest else None,
                rssi_avg_dbm=latest.rssi_avg_dbm if latest else None,
            )
        )
    return result


@router.get("/devices/{device_id}/config", response_model=DeviceConfigOut)
def get_device_config(device_id: str, db: Session = Depends(get_db)) -> DeviceConfigOut:
    device = db.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    return device_config_out(device)


@router.put("/devices/{device_id}/config", response_model=DeviceConfigOut)
def save_device_config(
    device_id: str,
    body: DeviceConfig,
    db: Session = Depends(get_db),
) -> DeviceConfigOut:
    """Save the admin copy only; the device is unchanged until push_config."""
    device = db.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    store_device_config(device, body)
    db.commit()
    db.refresh(device)
    return device_config_out(device)


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
    elif body.type == CommandType.PUSH_CONFIG:
        if payload:
            try:
                config = DeviceConfig.model_validate(payload)
            except ValidationError as exc:
                raise HTTPException(
                    status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                    detail=exc.errors(include_url=False),
                ) from exc
        elif device.config:
            config = DeviceConfig.model_validate(device.config)
        else:
            raise HTTPException(
                status_code=status.HTTP_409_CONFLICT,
                detail="no saved configuration for this device",
            )
        store_device_config(device, config)
        payload = config.model_dump(mode="json")
    elif body.type == CommandType.GET_CONFIG:
        payload = {}

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


@router.get("/archive.csv", response_class=PlainTextResponse)
def download_archive_csv(
    device_id: str,
    start: int,
    end: int,
    db: Session = Depends(get_db),
) -> PlainTextResponse:
    """CSV export of stored archive buckets for a device (max one week).

    ``start``/``end`` are Unix epochs in UTC. Each row carries the bucket end
    twice: once in UTC and once in the device's configured time zone.
    """
    if end <= start or end - start > 7 * 86400:
        raise HTTPException(status_code=422, detail="range must be positive and at most one week")
    device = db.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    start_dt = datetime.fromtimestamp(start, UTC)
    end_dt = datetime.fromtimestamp(end, UTC)
    rows = db.scalars(
        select(ArchiveRecord)
        .where(
            ArchiveRecord.device_id == device_id,
            ArchiveRecord.date_time >= start_dt,
            ArchiveRecord.date_time <= end_dt,
        )
        .order_by(ArchiveRecord.date_time)
    ).all()
    buffer = io.StringIO()
    writer = csv.writer(buffer)
    writer.writerow(
        [
            "date_time_utc",
            "date_time_site",
            "out_temp_c",
            "out_humidity_pct",
            "pressure_hpa",
            "wind_speed_m_s",
            "wind_dir_deg",
            "wind_gust_m_s",
            "rain_mm",
            "frames_valid",
            "frames_expected",
            "frames_crc_bad",
            "rssi_avg_dbm",
        ]
    )
    for row in rows:
        writer.writerow(
            [
                format_utc(row.date_time),
                format_in_zone(row.date_time, device.timezone),
                row.out_temp_c,
                row.out_humidity_pct,
                row.pressure_hpa,
                row.wind_speed_m_s,
                row.wind_dir_deg,
                row.wind_gust_m_s,
                row.rain_mm,
                row.frames_valid,
                row.frames_expected,
                row.frames_crc_bad,
                row.rssi_avg_dbm,
            ]
        )
    return PlainTextResponse(
        buffer.getvalue(),
        media_type="text/csv",
        headers={"Content-Disposition": f'attachment; filename="{device_id}-archive.csv"'},
    )


@router.get("/raw.csv", response_class=PlainTextResponse)
def download_raw_csv(
    device_id: str,
    start: int,
    end: int,
    request: Request,
    db: Session = Depends(get_db),
) -> PlainTextResponse:
    """Download a raw-frame CSV produced by the download_raw_data command.

    The stored file is UTC-only. The download adds a second timestamp column in
    the device's configured time zone.
    """
    device = db.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    path = raw_csv_path(request.app.state.settings.data_dir, device_id, start, end)
    if not path.exists():
        raise HTTPException(status_code=404, detail="no raw data for that range")
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise HTTPException(status_code=404, detail="raw file is empty")

    records: list[tuple[datetime, str]] = []
    for line in lines[1:]:
        stamp, _, rest = line.partition(",")
        try:
            when = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
        except ValueError:
            continue
        records.append((when, rest))
    records.sort(key=lambda item: item[0])
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["timestamp_utc", "timestamp_site", "raw", "rssi", "channel"])
    for when, rest in records:
        writer.writerow([format_utc(when), format_in_zone(when, device.timezone), *rest.split(",")])
    return PlainTextResponse(
        output.getvalue(),
        media_type="text/csv",
        headers={"Content-Disposition": f'attachment; filename="{device_id}-raw.csv"'},
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
        original = f"akashavani-{version}.bin"
    filename = f"akashavani-{version}-{original}"
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
