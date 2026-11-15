from __future__ import annotations

from tests.factories import DEVICE_ID, admin_headers, device_headers, observation_payload


def test_admin_can_create_device_and_use_its_token(client):
    created = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-002", "name": "Second simulator", "token": "second-device-token"},
        headers=admin_headers(),
    )
    assert created.status_code == 200, created.text
    assert created.json()["id"] == "sim-002"

    payload = observation_payload(device_id="sim-002")
    response = client.post(
        "/api/v1/observations",
        json=payload,
        headers=device_headers("second-device-token"),
    )
    assert response.status_code == 202, response.text


def test_token_cannot_belong_to_two_devices(client):
    response = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-003", "token": "dev-sim-token"},
        headers=admin_headers(),
    )
    assert response.status_code == 409


def test_token_rotation_replaces_old_token(client):
    first = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-004", "token": "first-token-123"},
        headers=admin_headers(),
    )
    assert first.status_code == 200
    second = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-004", "token": "second-token-456"},
        headers=admin_headers(),
    )
    assert second.status_code == 200

    old = client.get("/api/v1/devices/me", headers=device_headers("first-token-123"))
    new = client.get("/api/v1/devices/me", headers=device_headers("second-token-456"))
    assert old.status_code == 401
    assert new.status_code == 200


def test_device_timezone_defaults_to_utc(client):
    listed = client.get("/api/v1/admin/devices", headers=admin_headers()).json()
    assert listed[0]["timezone"] == "UTC"


def test_device_timezone_is_updated_from_config(client):
    response = client.put(
        "/api/v1/devices/me/config",
        json={
            "version": 1,
            "timezone": "Asia/Kolkata",
            "site": {"latitude": 13.0, "longitude": 77.0, "altitude_m": 900.0},
            "wifi": [],
        },
        headers=device_headers(),
    )
    assert response.status_code == 200, response.text

    listed = client.get("/api/v1/admin/devices", headers=admin_headers()).json()
    assert listed[0]["timezone"] == "Asia/Kolkata"
    assert listed[0]["altitude_m"] == 900.0


def test_device_timezone_rejects_unknown_zone(client):
    response = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-005", "token": "dev-sim-token-2", "timezone": "Mars/Olympus"},
        headers=admin_headers(),
    )
    assert response.status_code == 422


def test_token_rotation_keeps_configured_timezone(client):
    client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-006", "token": "timezone-token-1", "timezone": "Asia/Kolkata"},
        headers=admin_headers(),
    )
    rotated = client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-006", "token": "timezone-token-2"},
        headers=admin_headers(),
    )
    assert rotated.status_code == 200
    assert rotated.json()["timezone"] == "Asia/Kolkata"


def test_device_config_report_roundtrip(client):
    reported = client.put(
        "/api/v1/devices/me/config",
        json={
            "version": 1,
            "timezone": "Asia/Kolkata",
            "site": {"latitude": 13.1, "longitude": 77.2, "altitude_m": 910.0},
            "wifi": [{"ssid": "site-net", "password": "site-password"}],
            "admin_password": "bench-admin",
            "upload_enabled": True,
            "archive_enabled": True,
            "upload_interval_seconds": 120,
            "command_poll_interval_seconds": 45,
        },
        headers=device_headers(),
    )
    assert reported.status_code == 200, reported.text
    assert reported.json()["config_version"] == 2

    stored = client.get(f"/api/v1/admin/devices/{DEVICE_ID}/config", headers=admin_headers())
    assert stored.status_code == 200
    body = stored.json()
    assert body["config"]["timezone"] == "Asia/Kolkata"
    assert body["config"]["site"]["altitude_m"] == 910.0
    assert body["config"]["wifi"][0]["ssid"] == "site-net"
    assert body["config_updated_at"].endswith("Z")

    device = client.get("/api/v1/devices/me", headers=device_headers()).json()
    assert device["altitude_m"] == 910.0
    assert device["timezone"] == "Asia/Kolkata"


def test_admin_save_updates_server_copy(client):
    response = client.put(
        f"/api/v1/admin/devices/{DEVICE_ID}/config",
        json={
            "version": 1,
            "timezone": "UTC",
            "site": {"latitude": 1.0, "longitude": 2.0, "altitude_m": 100.0},
            "wifi": [],
        },
        headers=admin_headers(),
    )
    assert response.status_code == 200, response.text
    assert response.json()["config_version"] == 2

    device = client.get("/api/v1/devices/me", headers=device_headers()).json()
    assert device["altitude_m"] == 100.0
