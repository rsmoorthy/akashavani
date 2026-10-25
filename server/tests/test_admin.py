from __future__ import annotations

from tests.factories import admin_headers, device_headers, observation_payload


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
