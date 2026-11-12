from __future__ import annotations

from datetime import UTC, datetime, timedelta, timezone
from pathlib import Path

from fastapi import APIRouter, Depends, HTTPException, Request, status
from pydantic import BaseModel, Field
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Device
from ..security import require_device

router = APIRouter(prefix="/api/v1/devices/me", tags=["raw"])
IST = timezone(timedelta(hours=5, minutes=30))
MAX_RANGE_SECONDS = 7 * 86400
MAX_RECORDS_PER_BATCH = 200


class RawRecordIn(BaseModel):
    t: int = Field(ge=0)
    hex: str = Field(min_length=2, max_length=48)
    rssi: int | None = None
    ch: int | None = None


class RawBatchIn(BaseModel):
    start_epoch: int = Field(ge=0)
    end_epoch: int = Field(ge=0)
    records: list[RawRecordIn]


def raw_csv_path(data_dir: Path, device_id: str, start: int, end: int) -> Path:
    return data_dir / "raw" / f"{device_id}-{start}-{end}.csv"


@router.post("/raw", status_code=status.HTTP_202_ACCEPTED)
def ingest_raw(
    body: RawBatchIn,
    request: Request,
    device: Device = Depends(require_device),
    db: Session = Depends(get_db),
) -> dict[str, int]:
    """Append a batch of raw frames for the download_raw_data command."""
    if body.end_epoch <= body.start_epoch or body.end_epoch - body.start_epoch > MAX_RANGE_SECONDS:
        raise HTTPException(status_code=422, detail="range must be positive and at most one week")
    if len(body.records) > MAX_RECORDS_PER_BATCH:
        raise HTTPException(
            status_code=422, detail=f"at most {MAX_RECORDS_PER_BATCH} records per batch"
        )
    settings = request.app.state.settings
    path = raw_csv_path(settings.data_dir, device.id, body.start_epoch, body.end_epoch)
    path.parent.mkdir(parents=True, exist_ok=True)
    is_new = not path.exists()
    with path.open("a", encoding="utf-8") as handle:
        if is_new:
            handle.write("timestamp_ist,raw,rssi,channel\n")
        for record in body.records:
            when = datetime.fromtimestamp(record.t, UTC).astimezone(IST).isoformat()
            handle.write(
                f"{when},{record.hex},{'' if record.rssi is None else record.rssi},"
                f"{'' if record.ch is None else record.ch}\n"
            )
    db.commit()
    return {"stored": len(body.records)}
