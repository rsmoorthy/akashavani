from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter, Depends, HTTPException, Request, status
from fastapi.responses import FileResponse
from packaging.version import InvalidVersion, Version
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Device, FirmwareRelease
from ..schemas import FirmwareCheckResponse
from ..security import require_device

router = APIRouter(prefix="/api/v1/firmware", tags=["firmware"])


@router.get("/check", response_model=FirmwareCheckResponse)
def check_firmware(
    current_version: str,
    channel: str = "stable",
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> FirmwareCheckResponse:
    del device
    latest = db.scalar(
        select(FirmwareRelease)
        .where(FirmwareRelease.channel == channel)
        .order_by(FirmwareRelease.created_at.desc())
        .limit(1)
    )
    if latest is None:
        return FirmwareCheckResponse(update_available=False, version=current_version)

    try:
        update_available = Version(latest.version) > Version(current_version)
    except InvalidVersion:
        update_available = latest.version != current_version

    if not update_available:
        return FirmwareCheckResponse(
            update_available=False, version=current_version, mandatory=False
        )

    return FirmwareCheckResponse(
        update_available=True,
        version=latest.version,
        download_url=f"/api/v1/firmware/{latest.version}/download",
        sha256=latest.sha256,
        size_bytes=latest.size_bytes,
        mandatory=latest.mandatory,
    )


@router.get("/{version}/download", response_class=FileResponse)
def download_firmware(
    version: str,
    request: Request,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> FileResponse:
    del device
    release = db.get(FirmwareRelease, version)
    if release is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="release not found")

    firmware_dir = request.app.state.settings.firmware_dir.resolve()
    filename = Path(release.filename).name
    path = (firmware_dir / filename).resolve()
    if path.parent != firmware_dir or not path.is_file():
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="image unavailable")

    return FileResponse(
        path,
        media_type="application/octet-stream",
        filename=filename,
        headers={
            "X-Checksum-SHA256": release.sha256,
            "X-Firmware-Version": release.version,
        },
    )
