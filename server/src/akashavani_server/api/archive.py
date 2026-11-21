from __future__ import annotations

import json
import logging
from datetime import UTC, datetime, timedelta
from typing import Any, Literal

import cbor2
import msgpack  # type: ignore[import-untyped]
from fastapi import APIRouter, Depends, HTTPException, Request, status
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from sqlalchemy import func, select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import ArchiveRecord, Device, RawArchiveRecord
from ..schemas import (
    ArchiveAccepted,
    ArchiveBatchAccepted,
    ArchiveBatchItemOut,
    ArchiveEnvelope,
)
from ..security import require_api_token
from ..services import weather

logger = logging.getLogger(__name__)
router = APIRouter(prefix="/api/v1", tags=["archive"])

MAX_BATCH_RECORDS = 100


def _existing(
    db: Session, device_id: str, message_id: str, date_time: datetime
) -> ArchiveRecord | None:
    return db.scalar(
        select(ArchiveRecord).where(
            ArchiveRecord.device_id == device_id,
            (ArchiveRecord.message_id == message_id) | (ArchiveRecord.date_time == date_time),
        )
    )


def _ingest_one(db: Session, settings: Any, body: ArchiveEnvelope) -> ArchiveAccepted:
    """Store one archive bucket; duplicates return the existing row."""
    device = db.get(Device, body.device_id)
    if device is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="unknown device")

    date_time = datetime.fromtimestamp(body.bucket_end_epoch, UTC)
    now = datetime.now(UTC)
    if date_time > now + timedelta(seconds=settings.max_future_skew_seconds):
        raise HTTPException(status_code=422, detail="bucket_end_epoch is too far in the future")

    existing = _existing(db, body.device_id, body.message_id, date_time)
    if existing is not None:
        return ArchiveAccepted(
            accepted=True, duplicate=True, id=existing.id, date_time=existing.date_time
        )

    sums = body.sums
    counts = body.counts
    out_temp = weather.mean(sums.out_temp_c, counts.out_temp)
    out_humidity = weather.mean(sums.out_humidity_pct, counts.out_humidity)
    pressure = weather.mean(sums.pressure_hpa, counts.pressure)
    in_temp = weather.mean(sums.in_temp_c, counts.in_temp)
    in_humidity = weather.mean(sums.in_humidity_pct, counts.in_humidity)
    wind_speed = weather.mean(sums.wind_speed_m_s, counts.wind_speed)
    solar = weather.mean(sums.solar_wm2, counts.solar)
    uv = weather.mean(sums.uv_index, counts.uv)

    wind_dir = (
        weather.vector_direction(body.wind_dir.sin_sum, body.wind_dir.cos_sum)
        if counts.wind_dir > 0
        else None
    )
    wind_gust = body.wind_gust.max_m_s if counts.wind_gust > 0 else None
    wind_gust_dir = body.wind_gust.dir_deg if counts.wind_gust > 0 else None

    bucket_mm = settings.rain_bucket_mm
    rain_mm = body.rain.tips * bucket_mm if counts.rain > 0 else None
    rain_in = rain_mm / 25.4 if rain_mm is not None else None
    instant_raw = body.rain.rate_raw if body.rain.rate_valid else None
    instant_mm = weather.rain_rate_mm_h_from_raw(
        instant_raw, high_rate=body.rain.rate_high, bucket_mm=bucket_mm
    )
    instant_in = instant_mm / 25.4 if instant_mm is not None else None

    window_start = date_time - timedelta(seconds=900)
    previous_rain = db.scalar(
        select(func.sum(ArchiveRecord.rain_mm)).where(
            ArchiveRecord.device_id == body.device_id,
            ArchiveRecord.date_time > window_start,
            ArchiveRecord.date_time < date_time,
        )
    )
    if previous_rain is not None or rain_mm is not None:
        trailing_mm = float(previous_rain or 0.0) + float(rain_mm or 0.0)
        rain_rate_15m_mm = trailing_mm * 4.0
        rain_rate_15m_in = rain_rate_15m_mm / 25.4
    else:
        rain_rate_15m_mm = None
        rain_rate_15m_in = None

    dewpoint = weather.dewpoint_c(out_temp, out_humidity)
    windchill = weather.windchill_c(out_temp, wind_speed)
    heatindex = weather.heatindex_c(out_temp, out_humidity)
    humidex = weather.humidex_c(out_temp, out_humidity)
    app_temp = weather.apptemp_c(out_temp, out_humidity, wind_speed)
    in_dewpoint = weather.dewpoint_c(in_temp, in_humidity)
    site_altitude = device.altitude_m
    barometer = (
        weather.sealevel_pressure_hpa(pressure, site_altitude, out_temp)
        if site_altitude is not None
        else None
    )
    altimeter = (
        weather.altimeter_hpa(pressure, site_altitude) if site_altitude is not None else None
    )
    cloudbase = (
        weather.cloudbase_m(out_temp, out_humidity, site_altitude)
        if site_altitude is not None
        else None
    )
    windrun = weather.windrun_km(wind_speed, body.interval_minutes)
    rx_check = (
        100.0 * body.quality.frames_valid / body.quality.frames_expected
        if body.quality.frames_expected > 0
        else None
    )

    missing = [
        name
        for name, count in (
            ("out_temp", counts.out_temp),
            ("out_humidity", counts.out_humidity),
            ("pressure", counts.pressure),
            ("wind_speed", counts.wind_speed),
            ("wind_dir", counts.wind_dir),
            ("wind_gust", counts.wind_gust),
            ("rain", counts.rain),
            ("uv", counts.uv),
            ("solar", counts.solar),
        )
        if count == 0
    ]
    quality_flags = {
        "missing": missing,
        "derived": "weewx-compatible-v1",
        "site_altitude_m": site_altitude,
    }
    if site_altitude is None:
        quality_flags["missing_site_altitude"] = True

    source = body.source.model_dump(exclude_none=True)

    record = ArchiveRecord(
        device_id=body.device_id,
        message_id=body.message_id,
        date_time=date_time,
        received_at=now,
        interval_minutes=body.interval_minutes,
        boot_id=body.boot_id,
        uptime_ms=body.uptime_ms,
        units="SI",
        source=source,
        out_temp_c=out_temp,
        out_temp_count=counts.out_temp,
        out_humidity_pct=out_humidity,
        out_humidity_count=counts.out_humidity,
        pressure_hpa=pressure,
        pressure_count=counts.pressure,
        barometer_hpa=barometer,
        altimeter_hpa=altimeter,
        wind_speed_m_s=wind_speed,
        wind_speed_count=counts.wind_speed,
        wind_dir_deg=wind_dir,
        wind_dir_count=counts.wind_dir,
        wind_gust_m_s=wind_gust,
        wind_gust_dir_deg=wind_gust_dir,
        wind_gust_count=counts.wind_gust,
        rain_tips=body.rain.tips if counts.rain > 0 else None,
        rain_count=counts.rain,
        rain_mm=rain_mm,
        rain_in=rain_in,
        rain_rate_instant_mm_h=instant_mm,
        rain_rate_instant_in_h=instant_in,
        rain_rate_15m_mm_h=rain_rate_15m_mm,
        rain_rate_15m_in_h=rain_rate_15m_in,
        solar_wm2=solar,
        solar_count=counts.solar,
        uv_index=uv,
        uv_count=counts.uv,
        in_temp_c=in_temp,
        in_temp_count=counts.in_temp,
        in_humidity_pct=in_humidity,
        in_humidity_count=counts.in_humidity,
        in_dewpoint_c=in_dewpoint,
        dewpoint_c=dewpoint,
        windchill_c=windchill,
        heatindex_c=heatindex,
        humidex_c=humidex,
        app_temp_c=app_temp,
        windrun_km=windrun,
        cloudbase_m=cloudbase,
        max_solar_wm2=None,
        et_mm=None,
        battery_low=body.latest.battery_low,
        supercap_v=body.latest.supercap_v if counts.supercap > 0 else None,
        supercap_min_v=body.latest.supercap_min_v if counts.supercap > 0 else None,
        supercap_count=counts.supercap,
        rx_check_percent=rx_check,
        frames_expected=body.quality.frames_expected,
        frames_valid=body.quality.frames_valid,
        frames_crc_bad=body.quality.frames_crc_bad,
        crc_bad_rssi_min_dbm=body.quality.crc_bad_rssi_min_dbm,
        crc_bad_rssi_avg_dbm=body.quality.crc_bad_rssi_avg_dbm,
        crc_bad_rssi_max_dbm=body.quality.crc_bad_rssi_max_dbm,
        resync_count=body.quality.resync_count,
        search_count=body.quality.search_count,
        gap_slots=body.quality.gap_slots,
        max_gap_slots=body.quality.max_gap_slots,
        rssi_avg_dbm=body.quality.rssi_avg_dbm,
        rssi_min_dbm=body.quality.rssi_min_dbm,
        rssi_max_dbm=body.quality.rssi_max_dbm,
        quality_flags=quality_flags,
        derived_version="1",
    )
    raw_record = RawArchiveRecord(
        device_id=body.device_id,
        message_id=body.message_id,
        date_time=date_time,
        received_at=now,
        source=source,
        raw=body.model_dump(mode="json"),
    )
    db.add(record)
    db.add(raw_record)
    device.last_seen_at = now
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        existing = _existing(db, body.device_id, body.message_id, date_time)
        if existing is None:
            raise
        return ArchiveAccepted(
            accepted=True, duplicate=True, id=existing.id, date_time=existing.date_time
        )

    db.refresh(record)
    logger.info(
        "archive device=%s bucket=%s id=%s temp=%s rain_mm=%s",
        body.device_id,
        date_time.isoformat(),
        record.id,
        out_temp,
        rain_mm,
    )
    return ArchiveAccepted(accepted=True, duplicate=False, id=record.id, date_time=record.date_time)


@router.post(
    "/archive",
    response_model=ArchiveAccepted,
    status_code=status.HTTP_202_ACCEPTED,
    dependencies=[Depends(require_api_token)],
)
def ingest_archive(
    body: ArchiveEnvelope, request: Request, db: Session = Depends(get_db)
) -> ArchiveAccepted | JSONResponse:
    settings = request.app.state.settings
    result = _ingest_one(db, settings, body)
    if result.duplicate:
        return JSONResponse(status_code=status.HTTP_200_OK, content=result.model_dump(mode="json"))
    return result


@router.post(
    "/archive/batch",
    response_model=ArchiveBatchAccepted,
    status_code=status.HTTP_202_ACCEPTED,
    dependencies=[Depends(require_api_token)],
)
async def ingest_archive_batch(
    request: Request, db: Session = Depends(get_db)
) -> ArchiveBatchAccepted:
    """Bulk archive ingest used by catch-up and get_data_again.

    Accepts a JSON array or a CBOR array of archive envelopes and returns one
    outcome per record so the device can advance past accepted/duplicate rows.
    """
    settings = request.app.state.settings
    raw = await request.body()
    content_type = request.headers.get("content-type", "").split(";")[0].strip().lower()
    try:
        if content_type == "application/cbor":
            items = cbor2.loads(raw)
        elif content_type == "application/msgpack":
            items = msgpack.unpackb(raw, raw=False)
        elif content_type in ("application/json", ""):
            items = json.loads(raw)
        else:
            raise HTTPException(
                status_code=status.HTTP_415_UNSUPPORTED_MEDIA_TYPE,
                detail="expected application/json, application/cbor or application/msgpack",
            )
    except (cbor2.CBORDecodeError, msgpack.exceptions.ExtraData, ValueError) as exc:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST, detail=f"invalid body: {exc}"
        ) from exc

    if not isinstance(items, list):
        raise HTTPException(status_code=422, detail="expected an array of archive records")
    if len(items) > MAX_BATCH_RECORDS:
        raise HTTPException(
            status_code=422, detail=f"at most {MAX_BATCH_RECORDS} records per batch"
        )

    results: list[ArchiveBatchItemOut] = []
    accepted = duplicates = rejected = 0
    for item in items:
        message_id = item.get("message_id") if isinstance(item, dict) else None
        try:
            body = ArchiveEnvelope.model_validate(item)
        except ValidationError as exc:
            rejected += 1
            results.append(
                ArchiveBatchItemOut(
                    message_id=message_id,
                    status="rejected",
                    error=f"invalid envelope: {exc.errors()[0].get('msg', 'invalid')}",
                )
            )
            continue
        try:
            outcome = _ingest_one(db, settings, body)
        except HTTPException as exc:
            rejected += 1
            results.append(
                ArchiveBatchItemOut(
                    message_id=body.message_id, status="rejected", error=str(exc.detail)
                )
            )
            continue
        state: Literal["accepted", "duplicate"]
        if outcome.duplicate:
            duplicates += 1
            state = "duplicate"
        else:
            accepted += 1
            state = "accepted"
        results.append(
            ArchiveBatchItemOut(
                message_id=body.message_id,
                status=state,
                id=outcome.id,
                date_time=outcome.date_time,
            )
        )

    return ArchiveBatchAccepted(
        results=results, accepted=accepted, duplicates=duplicates, rejected=rejected
    )
