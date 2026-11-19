from __future__ import annotations

import logging
from datetime import UTC, datetime, timedelta

import cbor2
from fastapi import APIRouter, Depends, HTTPException, Request, status
from fastapi.encoders import jsonable_encoder
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Device, Observation
from ..schemas import ObservationAccepted, ObservationEnvelope
from ..security import require_device

logger = logging.getLogger(__name__)
router = APIRouter(prefix="/api/v1", tags=["observations"])


def _parse_body(request: Request, raw: bytes) -> ObservationEnvelope:
    content_type = request.headers.get("content-type", "").split(";")[0].strip().lower()
    try:
        if content_type == "application/cbor":
            decoded = cbor2.loads(raw)
            return ObservationEnvelope.model_validate(decoded)
        if content_type in {"application/json", ""}:
            return ObservationEnvelope.model_validate_json(raw)
    except ValidationError as exc:
        raise HTTPException(
            status_code=422,
            detail=jsonable_encoder(exc.errors(include_url=False)),
        ) from exc
    except (cbor2.CBORDecodeError, ValueError) as exc:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST, detail="could not decode body"
        ) from exc
    raise HTTPException(
        status_code=status.HTTP_415_UNSUPPORTED_MEDIA_TYPE,
        detail="expected application/json or application/cbor",
    )


@router.post(
    "/observations",
    status_code=status.HTTP_202_ACCEPTED,
    response_model=ObservationAccepted,
)
async def ingest_observation(
    request: Request,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> JSONResponse:
    settings = request.app.state.settings
    raw = await request.body()
    if not raw:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="empty body")
    if len(raw) > settings.max_body_bytes:
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=f"body exceeds {settings.max_body_bytes} bytes",
        )

    envelope = _parse_body(request, raw)
    if envelope.device_id != device.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="device_id does not match the authenticated device",
        )

    now = datetime.now(UTC)
    if envelope.observed_at > now + timedelta(seconds=settings.max_future_skew_seconds):
        raise HTTPException(
            status_code=422,
            detail="observed_at is too far in the future",
        )

    device.last_seen_at = now
    firmware_version = request.headers.get("X-Firmware-Version")
    if firmware_version:
        device.firmware_version = firmware_version[:32]

    existing = db.scalar(
        select(Observation).where(
            Observation.device_id == device.id,
            Observation.message_id == envelope.message_id,
        )
    )
    if existing is not None:
        db.commit()
        accepted = ObservationAccepted(accepted=True, duplicate=True, id=existing.id)
        return JSONResponse(status_code=status.HTTP_200_OK, content=accepted.model_dump())

    observation = Observation(
        device_id=device.id,
        message_id=envelope.message_id,
        observed_at=envelope.observed_at,
        received_at=now,
        sequence=envelope.sequence,
        source=envelope.source.model_dump(exclude_none=True),
        data=envelope.data,
    )
    db.add(observation)
    try:
        db.commit()
    except IntegrityError:
        # Concurrent retry won the unique constraint; return the stored record.
        db.rollback()
        existing = db.scalar(
            select(Observation).where(
                Observation.device_id == device.id,
                Observation.message_id == envelope.message_id,
            )
        )
        if existing is None:
            raise
        accepted = ObservationAccepted(accepted=True, duplicate=True, id=existing.id)
        return JSONResponse(status_code=status.HTTP_200_OK, content=accepted.model_dump())

    db.refresh(observation)
    logger.info(
        "observation device=%s message=%s id=%s",
        device.id,
        envelope.message_id,
        observation.id,
    )
    accepted = ObservationAccepted(accepted=True, duplicate=False, id=observation.id)
    return JSONResponse(status_code=status.HTTP_202_ACCEPTED, content=accepted.model_dump())
