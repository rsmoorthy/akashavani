from __future__ import annotations

from datetime import UTC, datetime
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import or_, select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Command, Device
from ..schemas import CommandAck, CommandOut, DeviceOut
from ..security import require_device

router = APIRouter(prefix="/api/v1/devices/me", tags=["commands"])


@router.get("", response_model=DeviceOut)
def me(device: Device = Depends(require_device)) -> Device:
    return device


@router.post("/commands/lease", response_model=list[CommandOut])
def lease_commands(
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
    limit: Annotated[int, Query(ge=1, le=50)] = 5,
) -> list[Command]:
    now = datetime.now(UTC)
    statement = (
        select(Command)
        .where(
            Command.device_id == device.id,
            Command.status == "pending",
            or_(Command.expires_at.is_(None), Command.expires_at > now),
        )
        .order_by(Command.created_at)
        .limit(limit)
    )
    if db.get_bind().dialect.name != "sqlite":
        statement = statement.with_for_update(skip_locked=True)

    commands = list(db.scalars(statement).all())
    for command in commands:
        command.status = "delivered"
        command.delivered_at = now
    db.commit()
    return commands


@router.post("/commands/{command_id}/ack", response_model=CommandOut)
def acknowledge_command(
    command_id: str,
    ack: CommandAck,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> Command:
    command = db.get(Command, command_id)
    if command is None or command.device_id != device.id:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="command not found")

    if command.status in {"acked", "failed"}:
        return command

    now = datetime.now(UTC)
    command.status = "acked" if ack.status == "ok" else "failed"
    command.result = ack.result
    command.error = ack.error
    command.acked_at = now
    db.commit()
    return command
