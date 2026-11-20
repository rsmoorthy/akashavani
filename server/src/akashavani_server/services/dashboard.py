from __future__ import annotations

import math
from collections import defaultdict
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from datetime import UTC, date, datetime, time, timedelta
from zoneinfo import ZoneInfo

from ..models import ArchiveRecord
from ..schemas import (
    HistoryCoverageOut,
    HistoryPointOut,
    HistorySummaryOut,
    PublicCurrentOut,
    PublicStationOut,
    SummaryMetricOut,
)
from ..timezones import as_utc, zone

PERIOD_DAY = "day"
PERIOD_WEEK = "week"
PERIOD_MONTH = "month"
PERIOD_CUSTOM = "custom"
PERIODS = {PERIOD_DAY, PERIOD_WEEK, PERIOD_MONTH, PERIOD_CUSTOM}

BUCKET_5_MIN = 300
BUCKET_HOUR = 3600
BUCKET_DAY = 86400


@dataclass(frozen=True)
class DateRange:
    period: str
    timezone: str
    start_date: date
    end_date: date
    start: datetime
    end: datetime
    bucket_seconds: int
    days_in_range: int
    includes_today: bool


def today_in_zone(timezone_name: str, now: datetime | None = None) -> date:
    current = now or datetime.now(UTC)
    return as_utc(current).astimezone(zone(timezone_name)).date()


def _midnight(day: date, tz: ZoneInfo) -> datetime:
    return datetime.combine(day, time.min, tzinfo=tz).astimezone(UTC)


def resolve_range(
    period: str,
    *,
    anchor: date | None,
    start_date: date | None,
    end_date: date | None,
    timezone_name: str,
    today: date,
    max_days: int,
) -> DateRange:
    """Resolve a user-facing range into local dates and UTC boundaries."""
    if period not in PERIODS:
        raise ValueError(f"period must be one of {', '.join(sorted(PERIODS))}")
    tz = zone(timezone_name)

    if period == PERIOD_CUSTOM:
        if start_date is None or end_date is None:
            raise ValueError("custom range requires start_date and end_date")
        first, last = start_date, end_date
    else:
        anchor_date = anchor or today
        if anchor_date > today:
            raise ValueError("date cannot be in the future")
        if period == PERIOD_DAY:
            first = last = anchor_date
        elif period == PERIOD_WEEK:
            first = anchor_date - timedelta(days=6)
            last = anchor_date
        else:  # month
            first = anchor_date.replace(day=1)
            if anchor_date.month == 12:
                last = date(anchor_date.year + 1, 1, 1) - timedelta(days=1)
            else:
                last = date(anchor_date.year, anchor_date.month + 1, 1) - timedelta(days=1)

    if last < first:
        raise ValueError("end_date must be on or after start_date")
    # A calendar-month view may include days after today; those points simply
    # have no data. Day/week/custom ranges may not end in the future.
    if period != PERIOD_MONTH and last > today:
        raise ValueError("end_date cannot be in the future")
    days = (last - first).days + 1
    if days > max_days:
        raise ValueError(f"range must be at most {max_days} days")

    if period == PERIOD_DAY:
        bucket = BUCKET_5_MIN
    elif period == PERIOD_WEEK:
        bucket = BUCKET_HOUR
    elif period == PERIOD_MONTH:
        bucket = BUCKET_DAY
    elif days <= 1:
        bucket = BUCKET_5_MIN
    elif days <= 7:
        bucket = BUCKET_HOUR
    else:
        bucket = BUCKET_DAY

    return DateRange(
        period=period,
        timezone=timezone_name,
        start_date=first,
        end_date=last,
        start=_midnight(first, tz),
        end=_midnight(last + timedelta(days=1), tz),
        bucket_seconds=bucket,
        days_in_range=days,
        includes_today=first <= today <= last,
    )


def local_date(value: datetime, timezone_name: str) -> date:
    return as_utc(value).astimezone(zone(timezone_name)).date()


def observation_time(record: ArchiveRecord) -> datetime:
    """Start of the archive interval.

    ``date_time`` is the bucket end reported by the receiver. Attributing a
    record to the day/hour it represents avoids moving the 00:00 bucket into
    the next day.
    """
    interval = record.interval_minutes or 5
    return as_utc(record.date_time) - timedelta(minutes=interval)


def local_day_bounds(day: date, timezone_name: str) -> tuple[datetime, datetime]:
    tz = zone(timezone_name)
    return _midnight(day, tz), _midnight(day + timedelta(days=1), tz)


def _mean(values: Iterable[float | None]) -> float | None:
    present = [value for value in values if value is not None]
    return sum(present) / len(present) if present else None


def _maximum(values: Iterable[float | None]) -> float | None:
    present = [value for value in values if value is not None]
    return max(present) if present else None


def _minimum(values: Iterable[float | None]) -> float | None:
    present = [value for value in values if value is not None]
    return min(present) if present else None


def _sum(values: Iterable[float | None]) -> float | None:
    present = [value for value in values if value is not None]
    return sum(present) if present else None


def circular_mean(values: Iterable[float | None]) -> float | None:
    present = [value for value in values if value is not None]
    if not present:
        return None
    radians = [math.radians(value) for value in present]
    sin_sum = sum(math.sin(value) for value in radians)
    cos_sum = sum(math.cos(value) for value in radians)
    if abs(sin_sum) < 1e-12 and abs(cos_sum) < 1e-12:
        return None
    return (math.degrees(math.atan2(sin_sum, cos_sum)) + 360.0) % 360.0


def display_pressure(record: ArchiveRecord) -> float | None:
    if record.barometer_hpa is not None:
        return record.barometer_hpa
    return record.pressure_hpa


def rain_rate(record: ArchiveRecord) -> float | None:
    if record.rain_rate_instant_mm_h is not None:
        return record.rain_rate_instant_mm_h
    return record.rain_rate_15m_mm_h


def _bucket_key(
    record: ArchiveRecord, bucket_seconds: int, timezone_name: str, anchor: datetime
) -> object:
    observed = observation_time(record)
    if bucket_seconds == BUCKET_DAY:
        return local_date(observed, timezone_name)
    # Anchor sub-day buckets at local midnight so an hourly chart in a
    # half-hour offset zone still shows local hour boundaries.
    epoch = int(observed.timestamp())
    anchor_epoch = int(as_utc(anchor).timestamp())
    offset = epoch - anchor_epoch
    return anchor_epoch + (offset // bucket_seconds) * bucket_seconds


def _bucket_starts(date_range: DateRange) -> list[datetime]:
    if date_range.bucket_seconds == BUCKET_DAY:
        current = date_range.start_date
        starts: list[datetime] = []
        tz = zone(date_range.timezone)
        while current <= date_range.end_date:
            starts.append(_midnight(current, tz))
            current += timedelta(days=1)
        return starts

    starts = []
    step = timedelta(seconds=date_range.bucket_seconds)
    current = date_range.start
    while current < date_range.end:
        starts.append(current)
        current += step
    return starts


def _point(timestamp: datetime, records: Sequence[ArchiveRecord]) -> HistoryPointOut:
    return HistoryPointOut(
        t=timestamp,
        out_temp_c=_mean(record.out_temp_c for record in records),
        app_temp_c=_mean(record.app_temp_c for record in records),
        dewpoint_c=_mean(record.dewpoint_c for record in records),
        out_humidity_pct=_mean(record.out_humidity_pct for record in records),
        wind_speed_m_s=_mean(record.wind_speed_m_s for record in records),
        wind_gust_m_s=_maximum(record.wind_gust_m_s for record in records),
        wind_dir_deg=circular_mean(record.wind_dir_deg for record in records),
        pressure_hpa=_mean(display_pressure(record) for record in records),
        barometer_hpa=_mean(record.barometer_hpa for record in records),
        rain_rate_mm_h=_maximum(rain_rate(record) for record in records),
        rain_mm=_sum(record.rain_mm for record in records),
        solar_wm2=_mean(record.solar_wm2 for record in records),
        uv_index=_mean(record.uv_index for record in records),
    )


def build_points(records: Sequence[ArchiveRecord], date_range: DateRange) -> list[HistoryPointOut]:
    grouped: dict[object, list[ArchiveRecord]] = defaultdict(list)
    for record in records:
        grouped[
            _bucket_key(
                record,
                date_range.bucket_seconds,
                date_range.timezone,
                date_range.start,
            )
        ].append(record)

    points: list[HistoryPointOut] = []
    for timestamp in _bucket_starts(date_range):
        if date_range.bucket_seconds == BUCKET_DAY:
            key: object = local_date(timestamp, date_range.timezone)
        else:
            key = int(as_utc(timestamp).timestamp())
        points.append(_point(timestamp, grouped.get(key, [])))
    return points


def _temperature_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [record.out_temp_c for record in records]
    return SummaryMetricOut(high=_maximum(values), low=_minimum(values), average=_mean(values))


def _dewpoint_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [record.dewpoint_c for record in records]
    return SummaryMetricOut(high=_maximum(values), low=_minimum(values), average=_mean(values))


def _humidity_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [record.out_humidity_pct for record in records]
    return SummaryMetricOut(high=_maximum(values), low=_minimum(values), average=_mean(values))


def _wind_speed_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [record.wind_speed_m_s for record in records]
    return SummaryMetricOut(high=_maximum(values), low=_minimum(values), average=_mean(values))


def _wind_gust_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [record.wind_gust_m_s for record in records]
    return SummaryMetricOut(high=_maximum(values), low=_minimum(values), average=_mean(values))


def _precip_rate_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [rain_rate(record) for record in records]
    nonzero = [value for value in values if value is not None and value > 0]
    return SummaryMetricOut(
        high=_maximum(values),
        low=None,
        average=(sum(nonzero) / len(nonzero)) if nonzero else None,
    )


def _precip_accumulation_summary(
    records: Sequence[ArchiveRecord], date_range: DateRange
) -> SummaryMetricOut:
    daily: dict[date, float] = {}
    for record in records:
        if record.rain_mm is None:
            continue
        day = local_date(observation_time(record), date_range.timezone)
        daily[day] = daily.get(day, 0.0) + record.rain_mm
    if not daily:
        return SummaryMetricOut()
    total = sum(daily.values())
    if date_range.days_in_range == 1:
        return SummaryMetricOut(total=total)
    nonzero = [value for value in daily.values() if value > 0]
    return SummaryMetricOut(
        high=max(daily.values()),
        average=(sum(nonzero) / len(nonzero)) if nonzero else None,
        total=total,
    )


def _wind_direction_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    return SummaryMetricOut(average=circular_mean(record.wind_dir_deg for record in records))


def _pressure_summary(records: Sequence[ArchiveRecord]) -> SummaryMetricOut:
    values = [display_pressure(record) for record in records]
    return SummaryMetricOut(high=_maximum(values), low=_minimum(values))


def build_summary(records: Sequence[ArchiveRecord], date_range: DateRange) -> HistorySummaryOut:
    return HistorySummaryOut(
        temperature=_temperature_summary(records),
        dew_point=_dewpoint_summary(records),
        humidity=_humidity_summary(records),
        precipitation_rate=_precip_rate_summary(records),
        precipitation_accumulation=_precip_accumulation_summary(records, date_range),
        wind_speed=_wind_speed_summary(records),
        wind_gust=_wind_gust_summary(records),
        wind_direction=_wind_direction_summary(records),
        pressure=_pressure_summary(records),
    )


def build_coverage(records: Sequence[ArchiveRecord], date_range: DateRange) -> HistoryCoverageOut:
    days_with_data = len(
        {local_date(observation_time(record), date_range.timezone) for record in records}
    )
    return HistoryCoverageOut(
        days_in_range=date_range.days_in_range,
        days_with_data=days_with_data,
        includes_today=date_range.includes_today,
    )


def current_from_record(record: ArchiveRecord | None) -> PublicCurrentOut | None:
    if record is None:
        return None
    return PublicCurrentOut(
        date_time=record.date_time,
        out_temp_c=record.out_temp_c,
        app_temp_c=record.app_temp_c,
        dewpoint_c=record.dewpoint_c,
        out_humidity_pct=record.out_humidity_pct,
        wind_speed_m_s=record.wind_speed_m_s,
        wind_gust_m_s=record.wind_gust_m_s,
        wind_dir_deg=record.wind_dir_deg,
        pressure_hpa=record.pressure_hpa,
        barometer_hpa=record.barometer_hpa,
        rain_rate_instant_mm_h=record.rain_rate_instant_mm_h,
        rain_rate_15m_mm_h=record.rain_rate_15m_mm_h,
        rain_mm=record.rain_mm,
        solar_wm2=record.solar_wm2,
        uv_index=record.uv_index,
    )


def station_from_device(
    *,
    device_id: str,
    slug: str,
    name: str,
    timezone_name: str,
    latitude: float | None,
    longitude: float | None,
    altitude_m: float | None,
    online: bool,
    last_seen_at: datetime | None,
    first_archive_at: datetime | None,
    latest_archive_at: datetime | None,
    site_date: date,
) -> PublicStationOut:
    return PublicStationOut(
        id=device_id,
        slug=slug,
        name=name,
        timezone=timezone_name,
        latitude=latitude,
        longitude=longitude,
        altitude_m=altitude_m,
        online=online,
        last_seen_at=last_seen_at,
        first_archive_at=first_archive_at,
        latest_archive_at=latest_archive_at,
        site_date=site_date,
    )
