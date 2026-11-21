from __future__ import annotations

import math

from sqlalchemy import inspect, select

from akashavani_server.models import RawArchiveRecord
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
    assert row["boot_id"] == payload["boot_id"]
    assert row["uptime_ms"] == payload["uptime_ms"]
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
    assert row["crc_bad_rssi_min_dbm"] == -102.0
    assert row["crc_bad_rssi_avg_dbm"] == -88.0
    assert row["crc_bad_rssi_max_dbm"] == -45.0


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


def test_archive_keeps_raw_envelope_in_separate_table(client, app):
    payload = archive_payload()
    assert client.post("/api/v1/archive", json=payload, headers=api_headers()).status_code == 202

    with app.state.session_factory() as session:
        rows = list(session.scalars(select(RawArchiveRecord)))
    assert len(rows) == 1
    raw = rows[0]
    assert raw.device_id == payload["device_id"]
    assert raw.message_id == payload["message_id"]
    assert raw.source == payload["source"]
    assert raw.raw["boot_id"] == payload["boot_id"]
    assert raw.raw["sums"]["out_temp_c"] == payload["sums"]["out_temp_c"]

    columns = {column["name"] for column in inspect(app.state.engine).get_columns("archive")}
    assert "raw" not in columns

    # A retry is a duplicate and must not add a second raw envelope.
    assert client.post("/api/v1/archive", json=payload, headers=api_headers()).status_code == 200
    with app.state.session_factory() as session:
        assert len(list(session.scalars(select(RawArchiveRecord)))) == 1


def test_archive_accepts_envelopes_without_boot_fields(client):
    payload = archive_payload()
    del payload["boot_id"]
    del payload["uptime_ms"]
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 202, response.text

    row = client.get("/api/v1/admin/archive", headers=admin_headers()).json()[0]
    assert row["boot_id"] is None
    assert row["uptime_ms"] is None


def test_archive_rejects_malformed_boot_identity(client):
    payload = archive_payload(boot_id="not-hex", uptime_ms=-1)
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 422


def test_archive_rejects_half_boot_identity(client):
    payload = archive_payload()
    del payload["uptime_ms"]
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 422


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


def test_accepts_empty_radio_bucket(client):
    payload = archive_payload(
        sums={key: 0.0 for key in archive_payload()["sums"]},
        counts={key: 0 for key in archive_payload()["counts"]},
        wind_dir={"sin_sum": 0.0, "cos_sum": 0.0, "speed_sum": 0.0},
        wind_gust={"max_m_s": 0.0, "dir_deg": 0.0},
        rain={"tips": 0, "rate_raw": 0, "rate_high": False, "rate_valid": False},
        quality={
            "frames_expected": 100,
            "frames_valid": 0,
            "frames_crc_bad": 0,
            "resync_count": 0,
            "search_count": 0,
            "gap_slots": 100,
            "max_gap_slots": 1,
            "rssi_avg_dbm": 0.0,
            "rssi_min_dbm": 0,
            "rssi_max_dbm": 0,
        },
    )
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 202, response.text

    rows = client.get("/api/v1/admin/archive", headers=admin_headers()).json()
    row = rows[-1]
    assert row["frames_valid"] == 0
    assert row["frames_expected"] == 100
    assert row["out_temp_c"] is None
    assert row["rx_check_percent"] == 0.0


def test_batch_accepts_json_and_reports_per_record(client):
    first = archive_payload()
    second = archive_payload(bucket_end_epoch=first["bucket_end_epoch"] - 300)
    bad = {"message_id": "broken"}
    response = client.post(
        "/api/v1/archive/batch",
        json=[first, first, second, bad],
        headers=api_headers(),
    )
    assert response.status_code == 202, response.text
    body = response.json()
    assert body["accepted"] == 2
    assert body["duplicates"] == 1
    assert body["rejected"] == 1
    assert [item["status"] for item in body["results"]] == [
        "accepted",
        "duplicate",
        "accepted",
        "rejected",
    ]


def test_batch_accepts_cbor(client):
    import cbor2

    first = archive_payload()
    payloads = [first, archive_payload(bucket_end_epoch=first["bucket_end_epoch"] - 300)]
    response = client.post(
        "/api/v1/archive/batch",
        content=cbor2.dumps(payloads),
        headers={**api_headers(), "Content-Type": "application/cbor"},
    )
    assert response.status_code == 202, response.text
    assert response.json()["accepted"] == 2


def test_batch_rejects_oversized_batch(client):
    response = client.post(
        "/api/v1/archive/batch",
        json=[archive_payload() for _ in range(101)],
        headers=api_headers(),
    )
    assert response.status_code == 422


def test_archive_csv_export(client):
    payload = archive_payload()
    assert client.post("/api/v1/archive", json=payload, headers=api_headers()).status_code == 202
    start = payload["bucket_end_epoch"] - 300
    end = payload["bucket_end_epoch"] + 300
    response = client.get(
        f"/api/v1/admin/archive.csv?device_id={payload['device_id']}&start={start}&end={end}",
        headers=admin_headers(),
    )
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/csv")
    assert "date_time_utc" in response.text
    assert "date_time_site" in response.text
    assert "boot_id" in response.text
    assert "uptime_ms" in response.text
    assert payload["device_id"] in response.headers["content-disposition"]


def test_archive_csv_rejects_long_range(client):
    response = client.get(
        "/api/v1/admin/archive.csv?device_id=sim-001&start=0&end=999999",
        headers=admin_headers(),
    )
    assert response.status_code == 422


def test_archive_without_site_altitude_stores_nulls(client):
    client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-002", "token": "no-site-token"},
        headers=admin_headers(),
    )
    payload = archive_payload(device_id="sim-002")
    response = client.post("/api/v1/archive", json=payload, headers=api_headers())
    assert response.status_code == 202, response.text

    stored = client.get("/api/v1/admin/archive?device_id=sim-002", headers=admin_headers()).json()
    assert stored[0]["barometer_hpa"] is None
    assert stored[0]["altimeter_hpa"] is None
    assert stored[0]["cloudbase_m"] is None
    assert stored[0]["quality_flags"]["missing_site_altitude"] is True


def test_batch_accepts_msgpack(client):
    import msgpack

    first = archive_payload()
    payloads = [first, archive_payload(bucket_end_epoch=first["bucket_end_epoch"] - 300)]
    response = client.post(
        "/api/v1/archive/batch",
        content=msgpack.packb(payloads),
        headers={**api_headers(), "Content-Type": "application/msgpack"},
    )
    assert response.status_code == 202, response.text
    assert response.json()["accepted"] == 2
