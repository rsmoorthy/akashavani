from __future__ import annotations

from datetime import UTC, datetime
from uuid import uuid4

DEVICE_ID = "sim-001"
DEVICE_TOKEN = "dev-sim-token"
ADMIN_TOKEN = "test-admin-token"


def utc_now_string() -> str:
    return datetime.now(UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def observation_payload(**overrides):
    payload = {
        "v": 1,
        "type": "observation",
        "device_id": DEVICE_ID,
        "message_id": f"msg-{uuid4().hex}",
        "observed_at": utc_now_string(),
        "sequence": 7,
        "source": {
            "protocol": "davis",
            "channel": 0,
            "transmitter_id": 1,
            "rssi_dbm": -72,
        },
        "data": {
            "temperature_c": 21.5,
            "humidity_pct": 55.0,
            "pressure_hpa": 1013.2,
            "wind_speed_ms": 2.5,
            "wind_direction_deg": 180.0,
            "rain_day_mm": 1.2,
        },
    }
    payload.update(overrides)
    return payload


def device_headers(token: str = DEVICE_TOKEN) -> dict[str, str]:
    return {"X-Device-Token": token, "X-Firmware-Version": "0.1.0-dev"}


def admin_headers(token: str = ADMIN_TOKEN) -> dict[str, str]:
    return {"Authorization": f"Bearer {token}"}
