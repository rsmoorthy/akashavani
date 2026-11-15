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


def test_push_config_uses_saved_copy(client):
    created = enqueue(client, "push_config")
    assert created.status_code == 200, created.text
    command_id = created.json()["id"]

    lease = client.post("/api/v1/devices/me/commands/lease", headers=device_headers())
    assert lease.status_code == 200
    commands = lease.json()
    assert len(commands) == 1
    assert commands[0]["type"] == "push_config"
    assert commands[0]["payload"]["timezone"] == "UTC"
    assert commands[0]["payload"]["site"]["altitude_m"] == 940.0
    assert commands[0]["payload"]["wifi"][0]["ssid"] == "bench-net"

    ack = client.post(
        f"/api/v1/devices/me/commands/{command_id}/ack",
        json={"status": "ok", "result": {"applied": True, "reboot_required": True}},
        headers=device_headers(),
    )
    assert ack.status_code == 200


def test_push_config_requires_a_saved_copy(client):
    client.post(
        "/api/v1/admin/devices",
        json={"id": "sim-007", "token": "no-config-token"},
        headers=admin_headers(),
    )
    response = client.post(
        "/api/v1/admin/devices/sim-007/commands",
        json={"type": "push_config", "payload": {}},
        headers=admin_headers(),
    )
    assert response.status_code == 409


def test_push_config_accepts_explicit_document(client):
    payload = {
        "version": 1,
        "timezone": "Asia/Kolkata",
        "site": {"latitude": 10.0, "longitude": 76.0, "altitude_m": 500.0},
        "wifi": [],
    }
    response = enqueue(client, "push_config", payload)
    assert response.status_code == 200, response.text
    assert response.json()["payload"]["site"]["altitude_m"] == 500.0

    stored = client.get(f"/api/v1/admin/devices/{DEVICE_ID}/config", headers=admin_headers()).json()
    assert stored["config"]["timezone"] == "Asia/Kolkata"


def test_get_config_carries_no_payload(client):
    response = enqueue(client, "get_config")
    assert response.status_code == 200, response.text
    assert response.json()["type"] == "get_config"
    assert response.json()["payload"] == {}


def test_device_reports_mac(client):
    headers = device_headers() | {"X-Device-Mac": "08:B6:1F:B8:38:50"}
    response = client.get("/api/v1/devices/me", headers=headers)
    assert response.status_code == 200
    body = response.json()
    assert "station_id" not in body
    assert body["mac_address"] == "08:b6:1f:b8:38:50"
    assert body["last_seen_at"] is not None


def test_enqueue_download_fills_release_details(client, app):
    from tests.test_firmware import register

    content = b"ota-image"
    assert register(client, app, version="0.2.0", content=content).status_code == 201
    created = client.post(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        json={"type": "download_new_fw", "payload": {"version": "0.2.0"}},
        headers=admin_headers(),
    )
    assert created.status_code == 200, created.text
    payload = created.json()["payload"]
    assert payload["version"] == "0.2.0"
    assert payload["url"] == "/api/v1/firmware/0.2.0/download"
    assert payload["size_bytes"] == len(content)
    assert len(payload["sha256"]) == 64


def test_enqueue_download_without_release_is_404(client):
    response = client.post(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        json={"type": "download_new_fw", "payload": {"version": "9.9.9"}},
        headers=admin_headers(),
    )
    assert response.status_code == 404


def test_list_commands_after_lease(client):
    command_id = enqueue(client).json()["id"]
    client.post("/api/v1/devices/me/commands/lease", headers=device_headers())
    listed = client.get(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        headers=admin_headers(),
    )
    assert listed.status_code == 200
    assert [item["id"] for item in listed.json()] == [command_id]
    assert listed.json()[0]["status"] == "delivered"


def test_device_reports_last_radio(client):
    epoch = 1794400000
    headers = device_headers() | {"X-Last-Radio-At": str(epoch)}
    response = client.get("/api/v1/devices/me", headers=headers)
    assert response.status_code == 200, response.text
    body = response.json()
    assert body["last_radio_at"] is not None
    assert body["last_radio_at"].startswith("2026-11-11T")


def test_last_radio_zero_is_ignored(client):
    headers = device_headers() | {"X-Last-Radio-At": "0"}
    response = client.get("/api/v1/devices/me", headers=headers)
    assert response.status_code == 200
    assert response.json()["last_radio_at"] is None


def test_only_one_command_in_flight(client):
    first = enqueue(client, "self_test").json()
    second = enqueue(client, "sync_time").json()

    lease = client.post("/api/v1/devices/me/commands/lease", headers=device_headers())
    assert [c["id"] for c in lease.json()] == [first["id"]]
    assert lease.json()[0]["attempts"] == 1

    blocked = client.post("/api/v1/devices/me/commands/lease", headers=device_headers())
    assert blocked.json() == []

    client.post(
        f"/api/v1/devices/me/commands/{first['id']}/ack",
        json={"status": "ok", "result": {"ok": True}},
        headers=device_headers(),
    )

    next_lease = client.post("/api/v1/devices/me/commands/lease", headers=device_headers())
    assert [c["id"] for c in next_lease.json()] == [second["id"]]


def test_ack_timeout_probes_then_requeues(client, app):
    app.state.settings.command_ack_timeout_seconds = 0
    command = enqueue(client, "self_test").json()

    first = client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json()
    assert first[0]["id"] == command["id"]
    assert first[0]["attempts"] == 1

    # The next lease times the delivery out and hands out the recovery probe.
    probe = client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json()
    assert len(probe) == 1
    assert probe[0]["type"] == "get_pending_commands"
    assert probe[0]["payload"]["for"] == command["id"]

    # The device no longer has the command (e.g. it rebooted): acknowledge the
    # probe with an empty list, then the original comes back.
    client.post(
        f"/api/v1/devices/me/commands/{probe[0]['id']}/ack",
        json={"status": "ok", "result": {"pending": []}},
        headers=device_headers(),
    )
    retried = client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json()
    assert retried[0]["id"] == command["id"]
    assert retried[0]["attempts"] == 2


def test_command_fails_after_three_attempts(client, app):
    app.state.settings.command_ack_timeout_seconds = 0
    command = enqueue(client, "self_test").json()

    for attempt in (1, 2):
        leased = client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json()
        assert leased[0]["id"] == command["id"]
        assert leased[0]["attempts"] == attempt
        probe = client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json()
        assert probe[0]["type"] == "get_pending_commands"
        client.post(
            f"/api/v1/devices/me/commands/{probe[0]['id']}/ack",
            json={"status": "ok", "result": {"pending": []}},
            headers=device_headers(),
        )

    third = client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json()
    assert third[0]["attempts"] == 3

    # No ack: the sweep marks it failed and hands out nothing.
    assert client.post("/api/v1/devices/me/commands/lease", headers=device_headers()).json() == []

    listed = client.get(
        f"/api/v1/admin/devices/{DEVICE_ID}/commands",
        headers=admin_headers(),
    ).json()
    failed = next(c for c in listed if c["id"] == command["id"])
    assert failed["status"] == "failed"
    assert "3 attempts" in failed["error"]
