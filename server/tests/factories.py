from __future__ import annotations

import time
from datetime import UTC, datetime
from uuid import uuid4

DEVICE_ID = "sim-001"
DEVICE_TOKEN = "dev-sim-token"
ADMIN_TOKEN = "test-admin-token"
API_TOKEN = "secret"


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


def api_headers(token: str = API_TOKEN) -> dict[str, str]:
    return {"Authorization": f"Bearer {token}", "Content-Type": "application/json"}


def archive_payload(**overrides):
    bucket_end = (int(time.time()) // 300 * 300) - 300
    payload = {
        "v": 1,
        "type": "archive",
        "device_id": DEVICE_ID,
        "message_id": f"archive-{uuid4().hex}",
        "bucket_end_epoch": bucket_end,
        "interval_minutes": 5,
        "source": {"protocol": "davis", "transmitter_id": 1},
        "sums": {
            "out_temp_c": 110.0,
            "out_humidity_pct": 275.0,
            "pressure_hpa": 4553.5,
            "in_temp_c": 130.0,
            "in_humidity_pct": 490.0,
            "wind_speed_m_s": 25.0,
            "uv_index": 30.0,
            "solar_wm2": 3500.0,
        },
        "counts": {
            "out_temp": 5,
            "out_humidity": 5,
            "pressure": 5,
            "in_temp": 5,
            "in_humidity": 5,
            "wind_speed": 10,
            "wind_dir": 10,
            "wind_gust": 1,
            "rain": 1,
            "solar": 5,
            "uv": 5,
            "supercap": 1,
        },
        "wind_dir": {"sin_sum": 10.0, "cos_sum": 0.0, "speed_sum": 25.0},
        "wind_gust": {"max_m_s": 4.0, "dir_deg": 90.0},
        "rain": {"tips": 2, "rate_raw": 720, "rate_high": False, "rate_valid": True},
        "latest": {"battery_low": False, "supercap_v": 3.1, "supercap_min_v": 3.0},
        "quality": {
            "frames_expected": 100,
            "frames_valid": 95,
            "frames_crc_bad": 2,
            "resync_count": 1,
            "search_count": 0,
            "gap_slots": 5,
            "max_gap_slots": 2,
            "rssi_avg_dbm": -55.0,
            "rssi_min_dbm": -70.0,
            "rssi_max_dbm": -40.0,
        },
    }
    payload.update(overrides)
    return payload
