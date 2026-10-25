from __future__ import annotations

from tests.factories import DEVICE_ID, admin_headers, device_headers


def enqueue(client, command_type: str = "sync_time", payload=None):
    return client.post(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        json={"type": command_type, "payload": payload or {}},
        headers=admin_headers(),
    )


def test_requires_admin_for_enqueue(client):
    response = client.post(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        json={"type": "reboot"},
    )
    assert response.status_code == 401

    response = client.post(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        json={"type": "reboot"},
        headers=admin_headers("wrong"),
    )
    assert response.status_code == 403


def test_lease_and_acknowledge(client):
    created = enqueue(client)
    assert created.status_code == 200, created.text
    command_id = created.json()["id"]
    assert created.json()["status"] == "pending"

    lease = client.post("/api/v1/devices/me/commands/lease?limit=5", headers=device_headers())
    assert lease.status_code == 200, lease.text
    commands = lease.json()
    assert len(commands) == 1
    assert commands[0]["id"] == command_id
    assert commands[0]["status"] == "delivered"
    assert commands[0]["type"] == "sync_time"

    empty = client.post("/api/v1/devices/me/commands/lease?limit=5", headers=device_headers())
    assert empty.status_code == 200
    assert empty.json() == []

    ack = client.post(
        f"/api/v1/devices/me/commands/{command_id}/ack",
        json={"status": "ok", "result": {"sntp": "synced"}},
        headers=device_headers(),
    )
    assert ack.status_code == 200, ack.text
    assert ack.json()["status"] == "acked"
    assert ack.json()["result"] == {"sntp": "synced"}
    assert ack.json()["acked_at"] is not None
    assert ack.json()["acked_at"].endswith("Z")


def test_ack_is_idempotent(client):
    command_id = enqueue(client).json()["id"]
    client.post("/api/v1/devices/me/commands/lease", headers=device_headers())
    first = client.post(
        f"/api/v1/devices/me/commands/{command_id}/ack",
        json={"status": "ok"},
        headers=device_headers(),
    )
    second = client.post(
        f"/api/v1/devices/me/commands/{command_id}/ack",
        json={"status": "error", "error": "late failure"},
        headers=device_headers(),
    )
    assert first.status_code == 200
    assert second.status_code == 200
    assert second.json()["status"] == "acked"


def test_failed_ack_requires_error(client):
    command_id = enqueue(client).json()["id"]
    response = client.post(
        f"/api/v1/devices/me/commands/{command_id}/ack",
        json={"status": "error"},
        headers=device_headers(),
    )
    assert response.status_code == 422


def test_unknown_command_ack_is_404(client):
    response = client.post(
        "/api/v1/devices/me/commands/not-a-command/ack",
        json={"status": "ok"},
        headers=device_headers(),
    )
    assert response.status_code == 404


def test_invalid_command_type_is_rejected(client):
    response = client.post(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        json={"type": "drop_database"},
        headers=admin_headers(),
    )
    assert response.status_code == 422


def test_device_identity(client):
    response = client.get("/api/v1/devices/me", headers=device_headers())
    assert response.status_code == 200
    assert response.json()["id"] == DEVICE_ID
    assert response.json()["name"] == "Simulator"
