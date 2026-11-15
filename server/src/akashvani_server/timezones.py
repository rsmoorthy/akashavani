from __future__ import annotations

from datetime import UTC, datetime
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

DEFAULT_DEVICE_TIMEZONE = "UTC"


def is_valid_timezone(name: str | None) -> bool:
    """True when ``name`` is an IANA time-zone identifier known to zoneinfo."""
    if not name or len(name) > 64:
        return False
    try:
        ZoneInfo(name)
    except (ZoneInfoNotFoundError, ValueError):
        return False
    return True


def zone(name: str | None) -> ZoneInfo:
    """Return the configured zone, falling back to UTC for unknown names."""
    if is_valid_timezone(name):
        return ZoneInfo(name)  # type: ignore[arg-type]
    return ZoneInfo(DEFAULT_DEVICE_TIMEZONE)


def as_utc(value: datetime) -> datetime:
    """Normalise a possibly-naive database value to an aware UTC datetime."""
    if value.tzinfo is None:
        return value.replace(tzinfo=UTC)
    return value.astimezone(UTC)


def format_in_zone(value: datetime, name: str | None, *, seconds: bool = True) -> str:
    """ISO-8601 rendering of ``value`` in the requested IANA time zone."""
    rendered = as_utc(value).astimezone(zone(name))
    if seconds:
        rendered = rendered.replace(microsecond=0)
    text = rendered.isoformat()
    return text[:-6] + "Z" if text.endswith("+00:00") else text


def format_utc(value: datetime) -> str:
    """ISO-8601 UTC rendering with a ``Z`` suffix."""
    rendered = as_utc(value).replace(microsecond=0)
    return rendered.isoformat().replace("+00:00", "Z")
