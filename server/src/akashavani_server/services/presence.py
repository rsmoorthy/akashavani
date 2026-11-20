from __future__ import annotations

from datetime import UTC, datetime

from ..timezones import as_utc

# Three 30-second device poll intervals. A device that has not contacted the
# server in this window is shown as offline.
ONLINE_WINDOW_SECONDS = 90


def is_online(last_seen_at: datetime | None, now: datetime | None = None) -> bool:
    if last_seen_at is None:
        return False
    current = now or datetime.now(UTC)
    return (as_utc(current) - as_utc(last_seen_at)).total_seconds() <= ONLINE_WINDOW_SECONDS
