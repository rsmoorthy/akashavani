from __future__ import annotations

import json
import shutil
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Annotated

from fastapi import APIRouter, Depends, Header, HTTPException, Request, status
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Command, Device
from ..security import require_device

router = APIRouter(prefix="/api/v1/devices/me/storage", tags=["storage"])

MAX_LIST_BYTES = 64 * 1024
MAX_CHUNK_BYTES = 8192
EXPORT_TTL_DAYS = 7


def _storage_path(path: str | None) -> str:
    value = (path or "").strip()
    if (
        not value
        or not value.startswith("/")
        or len(value) > 128
        or ".." in value.split("/")
        or "\\" in value
        or any(ord(char) < 0x20 for char in value)
    ):
        raise HTTPException(status_code=422, detail="invalid storage path")
    roots = ("/archive", "/raw", "/logs", "/state")
    if value != "/" and not any(value == root or value.startswith(root + "/") for root in roots):
        raise HTTPException(status_code=422, detail="storage path is not allowed")
    return value


def _command(db: Session, device: Device, command_id: str, expected_type: str) -> Command:
    command = db.get(Command, command_id)
    if command is None or command.device_id != device.id:
        raise HTTPException(status_code=404, detail="command not found")
    if command.type != expected_type:
        raise HTTPException(status_code=422, detail="command type mismatch")
    return command


def _export_dir(request: Request, device: Device, command: Command) -> Path:
    root = request.app.state.settings.data_dir / "storage" / device.id
    _cleanup(root)
    directory = root / command.id
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def _cleanup(root: Path) -> None:
    if not root.is_dir():
        return
    cutoff = datetime.now(UTC) - timedelta(days=EXPORT_TTL_DAYS)
    for child in root.iterdir():
        if not child.is_dir():
            continue
        modified = datetime.fromtimestamp(child.stat().st_mtime, UTC)
        if modified < cutoff:
            shutil.rmtree(child, ignore_errors=True)


def _write_meta(directory: Path, **values: object) -> None:
    meta = {
        "created_at": datetime.now(UTC).isoformat(),
        **values,
    }
    (directory / "meta.json").write_text(json.dumps(meta), encoding="utf-8")


@router.post("/list", status_code=status.HTTP_202_ACCEPTED)
async def upload_storage_list(
    request: Request,
    command_id: Annotated[str, Header(alias="X-Storage-Command")],
    path: Annotated[str | None, Header(alias="X-Storage-Path")] = None,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> dict[str, object]:
    command = _command(db, device, command_id, "sd_list")
    checked_path = _storage_path(path)
    raw = await request.body()
    if not raw or len(raw) > MAX_LIST_BYTES:
        raise HTTPException(status_code=413, detail="listing too large")
    try:
        listing = json.loads(raw)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail="invalid listing JSON") from exc
    if not isinstance(listing, dict):
        raise HTTPException(status_code=422, detail="listing must be an object")
    entries = listing.get("entries")
    if not isinstance(entries, list):
        raise HTTPException(status_code=422, detail="listing entries must be an array")

    directory = _export_dir(request, device, command)
    (directory / "listing.json").write_text(
        json.dumps(listing, separators=(",", ":")), encoding="utf-8"
    )
    _write_meta(
        directory,
        kind="listing",
        device_id=device.id,
        command_id=command.id,
        path=checked_path,
        size=len(raw),
        entries=len(entries),
        truncated=bool(listing.get("truncated")),
    )
    return {"accepted": True}


@router.post("/file", status_code=status.HTTP_202_ACCEPTED)
async def upload_storage_file(
    request: Request,
    command_id: Annotated[str, Header(alias="X-Storage-Command")],
    path: Annotated[str | None, Header(alias="X-Storage-Path")] = None,
    offset: Annotated[str | None, Header(alias="X-Storage-Offset")] = None,
    eof: Annotated[str | None, Header(alias="X-Storage-EOF")] = None,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> dict[str, object]:
    command = _command(db, device, command_id, "sd_read")
    checked_path = _storage_path(path)
    try:
        offset_value = int(offset or "0")
    except ValueError as exc:
        raise HTTPException(status_code=422, detail="invalid offset") from exc
    if offset_value < 0:
        raise HTTPException(status_code=422, detail="invalid offset")
    raw = await request.body()
    if not raw or len(raw) > MAX_CHUNK_BYTES:
        raise HTTPException(status_code=413, detail="file chunk too large")

    directory = _export_dir(request, device, command)
    (directory / "chunk.bin").write_bytes(raw)
    _write_meta(
        directory,
        kind="chunk",
        device_id=device.id,
        command_id=command.id,
        path=checked_path,
        offset=offset_value,
        eof=(eof or "0") in {"1", "true", "yes"},
        size=len(raw),
    )
    return {"accepted": True, "bytes": len(raw)}
