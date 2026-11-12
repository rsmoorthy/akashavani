from __future__ import annotations

import math

from tests.factories import admin_headers, api_headers, archive_payload


def test_accepts_archive_and_computes_si_values(client):
    payload = archive_payload()
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 202, response.text
    body = response.json()
    assert body["accepted"] is True
    assert body["duplicate"] is False

    rows = client.get("/api/v1/admin/archive", headers=admin_headers()).json()
    assert len(rows) == 1
    row = rows[0]
    assert row["units"] == "SI"
    assert row["out_temp_c"] == 22.0
    assert row["out_temp_count"] == 5
    assert row["out_humidity_pct"] == 55.0
    assert row["pressure_hpa"] == 910.7
    assert row["wind_speed_m_s"] == 2.5
    assert row["wind_dir_deg"] == 90.0
    assert row["wind_gust_m_s"] == 4.0
    assert row["rain_tips"] == 2
    assert math.isclose(row["rain_mm"], 0.508)
    assert math.isclose(row["rain_in"], 0.02)
    assert math.isclose(row["rain_rate_instant_mm_h"], 1.27)
    assert math.isclose(row["rain_rate_15m_mm_h"], 0.508 * 4)
    assert row["rx_check_percent"] == 95.0
    assert row["barometer_hpa"] is not None
    assert row["altimeter_hpa"] is not None
    assert row["dewpoint_c"] is not None
    assert row["windrun_km"] is not None
    assert row["in_temp_c"] == 26.0
    assert row["in_humidity_pct"] == 98.0
    assert row["in_dewpoint_c"] is not None
    assert row["battery_low"] is False
    assert row["supercap_v"] == 3.1


def test_archive_is_idempotent(client):
    payload = archive_payload()
    first = client.post("/api/v1/archive", json=payload, headers=api_headers())
    second = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert first.status_code == 202
    assert second.status_code == 200
    assert second.json()["duplicate"] is True
    assert second.json()["id"] == first.json()["id"]

    rows = client.get("/api/v1/admin/archive", headers=admin_headers()).json()
    assert len(rows) == 1


def test_archive_requires_bearer_token(client):
    response = client.post("/api/v1/archive", json=archive_payload())
    assert response.status_code == 401


def test_archive_rejects_wrong_bearer_token(client):
    response = client.post("/api/v1/archive", json=archive_payload(), headers=api_headers("wrong"))
    assert response.status_code == 403


def test_archive_can_include_missing_sensors(client):
    payload = archive_payload(
        sums={
            "out_temp_c": 0.0,
            "out_humidity_pct": 0.0,
            "pressure_hpa": 0.0,
            "in_temp_c": 0.0,
            "in_humidity_pct": 0.0,
            "wind_speed_m_s": 0.0,
            "uv_index": 0.0,
            "solar_wm2": 0.0,
        },
        counts={
            "out_temp": 0,
            "out_humidity": 0,
            "pressure": 0,
            "in_temp": 0,
            "in_humidity": 0,
            "wind_speed": 0,
            "wind_dir": 0,
            "wind_gust": 0,
            "rain": 0,
            "solar": 0,
            "uv": 0,
            "supercap": 0,
        },
        wind_dir={"sin_sum": 0.0, "cos_sum": 0.0, "speed_sum": 0.0},
        wind_gust={"max_m_s": None, "dir_deg": None},
        rain={"tips": 0, "rate_raw": None, "rate_high": False, "rate_valid": False},
        latest={"battery_low": True, "supercap_v": None, "supercap_min_v": None},
    )
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 202, response.text
    row = client.get("/api/v1/admin/archive", headers=admin_headers()).json()[0]
    assert row["out_temp_c"] is None
    assert row["rain_mm"] is None
    assert row["rain_rate_instant_mm_h"] is None
    assert row["wind_dir_deg"] is None
    assert row["battery_low"] is True
