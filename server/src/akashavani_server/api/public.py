from __future__ import annotations

import csv
import io
from datetime import date, datetime
from typing import Annotated, Literal

from fastapi import APIRouter, Depends, HTTPException, Query, Request, Response, status
from fastapi.responses import PlainTextResponse
from sqlalchemy import func, select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import ArchiveRecord, Device
from ..schemas import (
    PublicHistoryOut,
    PublicStationResponse,
    Slug,
)
from ..services.dashboard import (
    DateRange,
    build_coverage,
    build_points,
    build_summary,
    current_from_record,
    local_day_bounds,
    resolve_range,
    station_from_device,
    today_in_zone,
)
from ..services.presence import is_online
from ..timezones import format_in_zone, format_utc


def require_public_dashboard(request: Request) -> None:
    if not request.app.state.settings.public_dashboard_enabled:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="not found")


router = APIRouter(
    prefix="/api/v1/public",
    tags=["public"],
    dependencies=[Depends(require_public_dashboard)],
)


def _public_device(request: Request, db: Session, slug: str) -> Device:
    device = db.scalar(select(Device).where(Device.slug == slug))
    if device is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="station not found")
    allowlist = request.app.state.settings.public_device_id_list
    if allowlist and device.id not in allowlist:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="station not found")
    return device


def _rows(db: Session, device: Device, start: datetime, end: datetime) -> list[ArchiveRecord]:
    return list(
        db.scalars(
            select(ArchiveRecord)
            .where(
                ArchiveRecord.device_id == device.id,
                # date_time is the bucket end. The range boundaries describe
                # bucket starts, so the final bucket ends exactly at `end`.
                ArchiveRecord.date_time > start,
                ArchiveRecord.date_time <= end,
            )
            .order_by(ArchiveRecord.date_time)
        ).all()
    )


def _range_or_422(
    request: Request,
    device: Device,
    *,
    period: str,
    anchor: date | None,
    start_date: date | None,
    end_date: date | None,
) -> DateRange:
    settings = request.app.state.settings
    try:
        return resolve_range(
            period,
            anchor=anchor,
            start_date=start_date,
            end_date=end_date,
            timezone_name=device.timezone,
            today=today_in_zone(device.timezone),
            max_days=settings.public_max_range_days,
        )
    except ValueError as exc:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT, detail=str(exc)
        ) from exc


@router.get("/stations/{slug}", response_model=PublicStationResponse)
def get_station(
    slug: Slug,
    request: Request,
    response: Response,
    db: Session = Depends(get_db),
) -> PublicStationResponse:
    settings = request.app.state.settings
    device = _public_device(request, db, slug)
    site_date = today_in_zone(device.timezone)

    latest = db.scalar(
        select(ArchiveRecord)
        .where(ArchiveRecord.device_id == device.id)
        .order_by(ArchiveRecord.date_time.desc())
        .limit(1)
    )
    bounds = db.execute(
        select(func.min(ArchiveRecord.date_time), func.max(ArchiveRecord.date_time)).where(
            ArchiveRecord.device_id == device.id
        )
    ).one()
    first_archive_at, latest_archive_at = bounds[0], bounds[1]

    day_start, day_end = local_day_bounds(site_date, device.timezone)
    rain_total = db.scalar(
        select(func.sum(ArchiveRecord.rain_mm)).where(
            ArchiveRecord.device_id == device.id,
            ArchiveRecord.date_time > day_start,
            ArchiveRecord.date_time <= day_end,
        )
    )
    today_rain_mm = float(rain_total) if rain_total is not None else None

    response.headers["Cache-Control"] = f"public, max-age={settings.public_cache_seconds}"
    return PublicStationResponse(
        station=station_from_device(
            device_id=device.id,
            slug=device.slug,
            name=device.name,
            timezone_name=device.timezone,
            latitude=device.latitude,
            longitude=device.longitude,
            altitude_m=device.altitude_m,
            online=is_online(device.last_seen_at),
            last_seen_at=device.last_seen_at,
            first_archive_at=first_archive_at,
            latest_archive_at=latest_archive_at,
            site_date=site_date,
        ),
        current=current_from_record(latest),
        today_rain_mm=today_rain_mm,
    )


@router.get("/stations/{slug}/history", response_model=PublicHistoryOut)
def get_history(
    slug: Slug,
    request: Request,
    response: Response,
    period: Literal["day", "week", "month", "custom"] = "day",
    anchor: Annotated[date | None, Query(alias="date")] = None,
    start_date: date | None = None,
    end_date: date | None = None,
    db: Session = Depends(get_db),
) -> PublicHistoryOut:
    settings = request.app.state.settings
    device = _public_device(request, db, slug)
    date_range = _range_or_422(
        request,
        device,
        period=period,
        anchor=anchor,
        start_date=start_date,
        end_date=end_date,
    )
    rows = _rows(db, device, date_range.start, date_range.end)
    response.headers["Cache-Control"] = f"public, max-age={settings.public_cache_seconds}"
    return PublicHistoryOut(
        station_id=device.id,
        timezone=device.timezone,
        period=date_range.period,
        start_date=date_range.start_date,
        end_date=date_range.end_date,
        start=date_range.start,
        end=date_range.end,
        bucket_seconds=date_range.bucket_seconds,
        sample_count=len(rows),
        coverage=build_coverage(rows, date_range),
        points=build_points(rows, date_range),
        summary=build_summary(rows, date_range),
    )


@router.get("/stations/{slug}/export.csv", response_class=PlainTextResponse)
def export_csv(
    slug: Slug,
    request: Request,
    period: Literal["day", "week", "month", "custom"] = "day",
    anchor: Annotated[date | None, Query(alias="date")] = None,
    start_date: date | None = None,
    end_date: date | None = None,
    db: Session = Depends(get_db),
) -> PlainTextResponse:
    device = _public_device(request, db, slug)
    date_range = _range_or_422(
        request,
        device,
        period=period,
        anchor=anchor,
        start_date=start_date,
        end_date=end_date,
    )
    rows = _rows(db, device, date_range.start, date_range.end)

    buffer = io.StringIO()
    writer = csv.writer(buffer)
    writer.writerow(
        [
            "timestamp_utc",
            "timestamp_site",
            "out_temp_c",
            "app_temp_c",
            "dewpoint_c",
            "out_humidity_pct",
            "wind_speed_m_s",
            "wind_gust_m_s",
            "wind_dir_deg",
            "pressure_hpa",
            "barometer_hpa",
            "rain_rate_instant_mm_h",
            "rain_rate_15m_mm_h",
            "rain_mm",
            "solar_wm2",
            "uv_index",
        ]
    )
    for row in rows:
        writer.writerow(
            [
                format_utc(row.date_time),
                format_in_zone(row.date_time, device.timezone),
                row.out_temp_c,
                row.app_temp_c,
                row.dewpoint_c,
                row.out_humidity_pct,
                row.wind_speed_m_s,
                row.wind_gust_m_s,
                row.wind_dir_deg,
                row.pressure_hpa,
                row.barometer_hpa,
                row.rain_rate_instant_mm_h,
                row.rain_rate_15m_mm_h,
                row.rain_mm,
                row.solar_wm2,
                row.uv_index,
            ]
        )
    filename = f"akashavani-{device.slug}-{date_range.start_date}_{date_range.end_date}.csv"
    return PlainTextResponse(
        buffer.getvalue(),
        media_type="text/csv",
        headers={
            "Content-Disposition": f'attachment; filename="{filename}"',
            "Cache-Control": "no-store",
        },
    )
