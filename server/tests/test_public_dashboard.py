from __future__ import annotations

import json
from datetime import UTC, date, datetime, time, timedelta
from uuid import uuid4
from zoneinfo import ZoneInfo

import pytest
from fastapi.testclient import TestClient

from akashavani_server.main import create_app
from akashavani_server.models import ArchiveRecord, Device
from akashavani_server.services.dashboard import resolve_range, today_in_zone
from tests.factories import admin_headers, device_config

DEVICE_ID = "sim-001"
SLUG = "simulator"


def _set_device_config(client: TestClient, *, timezone: str = "Asia/Calcutta") -> None:
    response = client.put(
        f"/api/v1/admin/devices/{DEVICE_ID}/config",
        json=device_config(timezone=timezone),
        headers=admin_headers(),
    )
    assert response.status_code == 200, response.text


def _add_archive(app, when: datetime, **values) -> ArchiveRecord:  # type: ignore[no-untyped-def]
    with app.state.session_factory() as db:
        record = ArchiveRecord(
            device_id=DEVICE_ID,
            message_id=f"archive-{uuid4().hex}",
            date_time=when.astimezone(UTC),
            interval_minutes=5,
            units="SI",
            **values,
        )
        db.add(record)
        db.commit()
        db.refresh(record)
        return record


def _update_device(app, **values) -> None:  # type: ignore[no-untyped-def]
    with app.state.session_factory() as db:
        device = db.get(Device, DEVICE_ID)
        assert device is not None
        for key, value in values.items():
            setattr(device, key, value)
        db.commit()


def _local(day: date, hour: int, minute: int, tz: str = "Asia/Calcutta") -> datetime:
    return datetime.combine(day, time(hour, minute), tzinfo=ZoneInfo(tz))


def test_public_station_is_unauthenticated_and_has_no_private_fields(client):
    response = client.get(f"/api/v1/public/stations/{SLUG}")
    assert response.status_code == 200, response.text
    data = response.json()
    assert data["station"]["id"] == DEVICE_ID
    assert data["station"]["slug"] == SLUG
    assert data["station"]["name"] == "Simulator"
    assert data["station"]["timezone"] == "UTC"
    assert data["station"]["latitude"] == pytest.approx(13.4867)
    assert data["current"] is None
    assert data["today_rain_mm"] is None

    serialized = json.dumps(data).lower()
    for forbidden in ("token", "hash", "mac_address", "wifi", "admin_password", "config"):
        assert forbidden not in serialized


def test_public_station_reports_missing_site_as_null(client, app):
    _update_device(app, latitude=None, longitude=None, altitude_m=None)
    data = client.get(f"/api/v1/public/stations/{SLUG}").json()
    assert data["station"]["latitude"] is None
    assert data["station"]["longitude"] is None
    assert data["station"]["altitude_m"] is None


def test_public_station_online_window(client, app):
    now = datetime.now(UTC)
    _update_device(app, last_seen_at=now)
    assert client.get(f"/api/v1/public/stations/{SLUG}").json()["station"]["online"] is True
    _update_device(app, last_seen_at=now - timedelta(seconds=120))
    assert client.get(f"/api/v1/public/stations/{SLUG}").json()["station"]["online"] is False


def test_public_station_current_and_today_rain(client, app):
    now = datetime.now(UTC)
    site_today = today_in_zone("Asia/Calcutta", now)
    _set_device_config(client, timezone="Asia/Calcutta")
    _add_archive(
        app,
        _local(site_today, 9, 0),
        out_temp_c=25.5,
        out_humidity_pct=55.0,
        pressure_hpa=910.0,
        rain_mm=1.25,
        solar_wm2=600.0,
    )
    data = client.get(f"/api/v1/public/stations/{SLUG}").json()
    assert data["current"]["out_temp_c"] == pytest.approx(25.5)
    assert data["current"]["rain_mm"] == pytest.approx(1.25)
    assert data["today_rain_mm"] == pytest.approx(1.25)
    assert data["station"]["latest_archive_at"] is not None


def test_unknown_station_is_404(client):
    assert client.get("/api/v1/public/stations/not-a-station").status_code == 404
    assert client.get("/api/v1/public/stations/assets").status_code == 422


def test_public_station_list(client):
    response = client.get("/api/v1/public/stations")
    assert response.status_code == 200, response.text
    assert response.json() == [{"slug": SLUG, "name": "Simulator"}]
    assert response.headers["cache-control"] == "public, max-age=60"


def test_public_station_list_is_sorted_and_minimal(client):
    created = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-002", "name": "Garden", "token": "garden-device-token"},
        headers=admin_headers(),
    )
    assert created.status_code == 200, created.text
    data = client.get("/api/v1/public/stations").json()
    assert [item["slug"] for item in data] == ["garden", "simulator"]
    assert set(data[0]) == {"slug", "name"}


def test_root_redirects_to_dashboard(client):
    response = client.get("/", follow_redirects=False)
    assert response.status_code == 307
    assert response.headers["location"] == "/dashboard"


def test_monitoring_url_is_saved_cleared_and_published(client):
    url = "https://status.example.net/simulator"
    updated = client.patch(
        f"/api/v1/admin/devices/{DEVICE_ID}",
        json={"monitoring_url": url},
        headers=admin_headers(),
    )
    assert updated.status_code == 200, updated.text
    assert updated.json()["monitoring_url"] == url
    assert updated.json()["name"] == "Simulator"

    listed = client.get("/api/v1/admin/devices", headers=admin_headers()).json()
    seeded = next(device for device in listed if device["id"] == DEVICE_ID)
    assert seeded["monitoring_url"] == url

    name_only = client.patch(
        f"/api/v1/admin/devices/{DEVICE_ID}",
        json={"name": "Simulator renamed"},
        headers=admin_headers(),
    )
    assert name_only.status_code == 200, name_only.text
    assert name_only.json()["monitoring_url"] == url

    station = client.get(f"/api/v1/public/stations/{SLUG}").json()["station"]
    assert station["monitoring_url"] == url

    cleared = client.patch(
        f"/api/v1/admin/devices/{DEVICE_ID}",
        json={"monitoring_url": ""},
        headers=admin_headers(),
    )
    assert cleared.status_code == 200, cleared.text
    assert cleared.json()["monitoring_url"] is None
    station = client.get(f"/api/v1/public/stations/{SLUG}").json()["station"]
    assert station["monitoring_url"] is None


def test_monitoring_url_rejects_non_http_values(client):
    for value in ("ftp://status.example.net", "not a url", "javascript:alert(1)"):
        response = client.patch(
            f"/api/v1/admin/devices/{DEVICE_ID}",
            json={"monitoring_url": value},
            headers=admin_headers(),
        )
        assert response.status_code == 422, value


def test_day_history_and_summary(client, app):
    _set_device_config(client, timezone="Asia/Calcutta")
    today = today_in_zone("Asia/Calcutta")
    anchor = today - timedelta(days=1)
    # A bucket ending exactly at local midnight belongs to the previous day and
    # must not leak into this day's summary.
    _add_archive(app, _local(anchor, 0, 0), out_temp_c=99.0)
    _add_archive(
        app,
        _local(anchor, 0, 5),
        out_temp_c=20.0,
        dewpoint_c=10.0,
        out_humidity_pct=50.0,
        wind_speed_m_s=1.0,
        wind_gust_m_s=3.0,
        wind_dir_deg=350.0,
        pressure_hpa=900.0,
        rain_rate_instant_mm_h=0.0,
        rain_mm=1.0,
    )
    _add_archive(
        app,
        _local(anchor, 0, 10),
        out_temp_c=24.0,
        dewpoint_c=14.0,
        out_humidity_pct=60.0,
        wind_speed_m_s=3.0,
        wind_gust_m_s=5.0,
        wind_dir_deg=10.0,
        pressure_hpa=910.0,
        rain_rate_instant_mm_h=2.0,
        rain_mm=0.5,
    )
    _add_archive(
        app,
        _local(anchor, 1, 5),
        out_temp_c=22.0,
        dewpoint_c=12.0,
        out_humidity_pct=55.0,
        wind_speed_m_s=2.0,
        wind_gust_m_s=4.0,
        pressure_hpa=905.0,
        rain_rate_instant_mm_h=4.0,
    )

    response = client.get(f"/api/v1/public/stations/{SLUG}/history?period=day&date={anchor}")
    assert response.status_code == 200, response.text
    data = response.json()
    assert data["bucket_seconds"] == 300
    assert data["sample_count"] == 3
    assert len(data["points"]) == 288
    assert data["coverage"] == {
        "days_in_range": 1,
        "days_with_data": 1,
        "includes_today": False,
    }
    assert data["points"][0]["out_temp_c"] == pytest.approx(20.0)
    assert sum(point["out_temp_c"] is not None for point in data["points"]) == 3

    summary = data["summary"]
    assert summary["temperature"] == {
        "high": 24.0,
        "low": 20.0,
        "average": 22.0,
        "total": None,
    }
    assert summary["precipitation_rate"]["high"] == pytest.approx(4.0)
    assert summary["precipitation_rate"]["average"] == pytest.approx(3.0)
    assert summary["precipitation_rate"]["low"] is None
    assert summary["precipitation_accumulation"]["total"] == pytest.approx(1.5)
    assert summary["wind_direction"]["average"] == pytest.approx(0.0, abs=0.01)
    assert summary["pressure"] == {
        "high": 910.0,
        "low": 900.0,
        "average": None,
        "total": None,
    }


def test_week_and_month_buckets(client, app):
    _set_device_config(client, timezone="Asia/Calcutta")
    today = today_in_zone("Asia/Calcutta")
    anchor = today - timedelta(days=2)
    for offset in range(3):
        _add_archive(app, _local(anchor - timedelta(days=offset), 12, 0), out_temp_c=20.0)

    week = client.get(f"/api/v1/public/stations/{SLUG}/history?period=week&date={anchor}").json()
    assert week["bucket_seconds"] == 3600
    assert week["coverage"]["days_in_range"] == 7
    assert week["coverage"]["days_with_data"] == 3
    assert len(week["points"]) == 7 * 24

    month = client.get(f"/api/v1/public/stations/{SLUG}/history?period=month&date={anchor}").json()
    assert month["bucket_seconds"] == 86400
    assert month["coverage"]["days_in_range"] >= 28


def test_custom_range_and_validation(client, app):
    _set_device_config(client, timezone="Asia/Calcutta")
    today = today_in_zone("Asia/Calcutta")
    start = today - timedelta(days=2)
    response = client.get(
        f"/api/v1/public/stations/{SLUG}/history"
        f"?period=custom&start_date={start}&end_date={today}"
    )
    assert response.status_code == 200, response.text
    assert response.json()["bucket_seconds"] == 3600
    assert response.json()["coverage"]["days_in_range"] == 3

    one_day = client.get(
        f"/api/v1/public/stations/{SLUG}/history"
        f"?period=custom&start_date={today}&end_date={today}"
    )
    assert one_day.json()["bucket_seconds"] == 300

    long_start = today - timedelta(days=32)
    too_long = client.get(
        f"/api/v1/public/stations/{SLUG}/history"
        f"?period=custom&start_date={long_start}&end_date={today}"
    )
    assert too_long.status_code == 422

    future = client.get(
        f"/api/v1/public/stations/{SLUG}/history?period=day&date={today + timedelta(days=1)}"
    )
    assert future.status_code == 422

    inverted = client.get(
        f"/api/v1/public/stations/{SLUG}/history"
        f"?period=custom&start_date={today}&end_date={start}"
    )
    assert inverted.status_code == 422


def test_multi_day_precipitation_accumulation(client, app):
    _set_device_config(client, timezone="Asia/Calcutta")
    today = today_in_zone("Asia/Calcutta")
    first = today - timedelta(days=1)
    _add_archive(app, _local(first, 12, 0), rain_mm=1.0)
    _add_archive(app, _local(today, 12, 0), rain_mm=2.0)
    data = client.get(
        f"/api/v1/public/stations/{SLUG}/history"
        f"?period=custom&start_date={first}&end_date={today}"
    ).json()
    accumulation = data["summary"]["precipitation_accumulation"]
    assert accumulation["high"] == pytest.approx(2.0)
    assert accumulation["average"] == pytest.approx(1.5)
    assert accumulation["total"] == pytest.approx(3.0)


def test_dst_boundaries() -> None:
    spring = resolve_range(
        "day",
        anchor=date(2026, 3, 8),
        start_date=None,
        end_date=None,
        timezone_name="America/New_York",
        today=date(2026, 11, 20),
        max_days=31,
    )
    assert spring.end - spring.start == timedelta(hours=23)

    autumn = resolve_range(
        "day",
        anchor=date(2026, 11, 1),
        start_date=None,
        end_date=None,
        timezone_name="America/New_York",
        today=date(2026, 11, 20),
        max_days=31,
    )
    assert autumn.end - autumn.start == timedelta(hours=25)


def test_csv_export_is_public_and_metric(client, app):
    _set_device_config(client, timezone="Asia/Calcutta")
    today = today_in_zone("Asia/Calcutta")
    anchor = today - timedelta(days=1)
    _add_archive(
        app,
        _local(anchor, 9, 0),
        out_temp_c=25.5,
        pressure_hpa=910.0,
        rain_mm=1.25,
    )
    response = client.get(f"/api/v1/public/stations/{SLUG}/export.csv?period=day&date={anchor}")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/csv")
    assert "attachment" in response.headers["content-disposition"]
    lines = response.text.splitlines()
    assert len(lines) == 2
    assert lines[0].startswith("timestamp_utc,timestamp_site,out_temp_c")
    assert "25.5" in lines[1]
    assert "910.0" in lines[1]

    empty = client.get(
        f"/api/v1/public/stations/{SLUG}/export.csv?period=day&date={today - timedelta(days=5)}"
    )
    assert empty.status_code == 200
    assert len(empty.text.splitlines()) == 1


def test_public_dashboard_can_be_disabled(settings, client):
    disabled = settings.model_copy(update={"public_dashboard_enabled": False})
    with TestClient(create_app(disabled)) as other:
        assert other.get(f"/api/v1/public/stations/{SLUG}").status_code == 404
        assert other.get("/api/v1/public/stations").status_code == 404
        assert other.get("/dashboard").status_code == 404
        assert other.get("/dashboard/simulator/").status_code == 404


def test_public_device_allowlist(settings, client):
    restricted = settings.model_copy(update={"public_device_ids": "some-other-device"})
    with TestClient(create_app(restricted)) as other:
        assert other.get(f"/api/v1/public/stations/{SLUG}").status_code == 404
        assert other.get("/api/v1/public/stations").json() == []


def test_dashboard_html_routes(client):
    root = client.get("/dashboard", follow_redirects=False)
    assert root.status_code == 307
    assert root.headers["location"] == "/dashboard/simulator/"
    page = client.get("/dashboard/simulator/")
    assert page.status_code == 200
    assert "test dashboard" in page.text
    assert page.headers["cache-control"] == "no-cache"
    # The SPA is served for an unknown slug; the API produces the 404 state.
    assert client.get("/dashboard/unknown/").status_code == 200
    assert client.get("/dashboard/favicon.svg").status_code == 404


def test_admin_slug_generation_uniqueness_and_rename(client):
    first = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-010", "name": "Roof Station", "token": "sim-010-token"},
        headers=admin_headers(),
    )
    assert first.status_code == 200, first.text
    assert first.json()["slug"] == "roof-station"

    second = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-011", "name": "Roof Station", "token": "sim-011-token"},
        headers=admin_headers(),
    )
    assert second.status_code == 200, second.text
    assert second.json()["slug"] == "roof-station-2"

    duplicate = client.patch(
        "/api/v1/admin/devices/sim-011",
        json={"slug": "roof-station"},
        headers=admin_headers(),
    )
    assert duplicate.status_code == 409

    renamed = client.patch(
        "/api/v1/admin/devices/sim-011",
        json={"name": "Anemometer Mast", "slug": "mast"},
        headers=admin_headers(),
    )
    assert renamed.status_code == 200, renamed.text
    assert renamed.json()["slug"] == "mast"
    assert client.get("/api/v1/public/stations/mast").json()["station"]["name"] == "Anemometer Mast"

    name_only = client.patch(
        "/api/v1/admin/devices/sim-011",
        json={"name": "Mast Two"},
        headers=admin_headers(),
    )
    assert name_only.json()["slug"] == "mast"

    reserved = client.patch(
        "/api/v1/admin/devices/sim-011",
        json={"slug": "assets"},
        headers=admin_headers(),
    )
    assert reserved.status_code == 422


def test_seeded_device_has_slug(client):
    listed = client.get("/api/v1/admin/devices", headers=admin_headers()).json()
    seeded = next(device for device in listed if device["id"] == DEVICE_ID)
    assert seeded["slug"] == SLUG


def test_public_dashboard_cache_header(client):
    response = client.get(f"/api/v1/public/stations/{SLUG}")
    assert response.headers["cache-control"] == "public, max-age=60"
