from __future__ import annotations

from datetime import UTC, datetime, timedelta
from typing import Annotated
from uuid import uuid4

from fastapi import APIRouter, Depends, HTTPException, Query, Request, status
from sqlalchemy import or_, select
from sqlalchemy.orm import Session

from ..config import Settings
from ..db import get_db
from ..models import Command, Device
from ..schemas import CommandAck, CommandOut, DeviceOut
from ..security import require_device

router = APIRouter(prefix="/api/v1/devices/me", tags=["commands"])

MAX_ATTEMPTS = 3


@router.get("", response_model=DeviceOut)
def me(device: Device = Depends(require_device)) -> Device:
    return device


@router.post("/commands/lease", response_model=list[CommandOut])
def lease_commands(
    request: Request,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
    limit: Annotated[int, Query(ge=1, le=50)] = 5,
) -> list[Command]:
    """Deliver at most one command, after sweeping lost acknowledgements.

    A command is in flight while its status is ``delivered``; no second command
    is handed out. Delivered commands that exceed the ACK timeout are marked
    ``timed_out``, a ``get_pending_commands`` probe is queued, and a retry is
    decided when the probe is acknowledged.
    """
    settings = request.app.state.settings
    now = datetime.now(UTC)
    timeout = timedelta(seconds=settings.command_ack_timeout_seconds)

    delivered = list(
        db.scalars(
            select(Command).where(Command.device_id == device.id, Command.status == "delivered")
        ).all()
    )
    changed = False
    for command in delivered:
        delivered_at = command.delivered_at
        if delivered_at is not None and delivered_at.tzinfo is None:
            delivered_at = delivered_at.replace(tzinfo=UTC)
        if delivered_at is not None and now - delivered_at < timeout:
            continue
        changed = True
        if command.type == "get_pending_commands":
            command.status = "failed"
            command.error = "ack timeout"
            command.acked_at = now
        elif command.attempts >= MAX_ATTEMPTS:
            command.status = "failed"
            command.error = f"no ack after {command.attempts} attempts"
            command.acked_at = now
        else:
            command.status = "timed_out"
            probe = db.scalar(
                select(Command).where(
                    Command.device_id == device.id,
                    Command.type == "get_pending_commands",
                    Command.status.in_(("pending", "delivered")),
                )
            )
            if probe is None:
                db.add(
                    Command(
                        id=str(uuid4()),
                        device_id=device.id,
                        type="get_pending_commands",
                        payload={"for": command.id},
                        status="pending",
                        expires_at=now + timedelta(seconds=settings.command_default_ttl_seconds),
                    )
                )
    if changed:
        db.commit()

    # One command in flight: never deliver while something is delivered.
    in_flight = db.scalar(
        select(Command).where(Command.device_id == device.id, Command.status == "delivered")
    )
    if in_flight is not None:
        return []

    statement = (
        select(Command)
        .where(
            Command.device_id == device.id,
            Command.status == "pending",
            or_(Command.expires_at.is_(None), Command.expires_at > now),
        )
        .order_by(Command.created_at)
        .limit(1)
    )
    if db.get_bind().dialect.name != "sqlite":
        statement = statement.with_for_update(skip_locked=True)
    commands = list(db.scalars(statement).all())
    for command in commands:
        command.status = "delivered"
        command.delivered_at = now
        command.last_attempt_at = now
        command.attempts += 1
    db.commit()
    return commands


@router.post("/commands/{command_id}/ack", response_model=CommandOut)
def acknowledge_command(
    request: Request,
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

    if command.type == "get_pending_commands" and ack.status == "ok":
        _resolve_probe(db, command, ack, now, request.app.state.settings)

    db.commit()
    return command


def _resolve_probe(
    db: Session, probe: Command, ack: CommandAck, now: datetime, settings: Settings
) -> None:
    """After a probe, requeue a timed-out command the device no longer has."""
    for_id = (probe.payload or {}).get("for")
    if not for_id:
        return
    original = db.get(Command, str(for_id))
    if original is None or original.status != "timed_out":
        return
    if original.attempts >= MAX_ATTEMPTS:
        original.status = "failed"
        original.error = f"no ack after {original.attempts} attempts"
        original.acked_at = now
        return
    pending_ids = (ack.result or {}).get("pending", [])
    if isinstance(pending_ids, list) and for_id in pending_ids:
        # The device still has it; extend the delivery window.
        original.status = "delivered"
        original.delivered_at = now
        return
    original.status = "pending"
    original.delivered_at = None
    original.expires_at = now + timedelta(seconds=settings.command_default_ttl_seconds)
