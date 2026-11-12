from __future__ import annotations

from datetime import UTC, datetime, timedelta

import cbor2

from tests.factories import DEVICE_ID, admin_headers, device_headers, observation_payload


def test_accepts_json_observation(client):
    payload = observation_payload()
    response = client.post("/api/v1/observations", json=payload, headers=device_headers())
    assert response.status_code == 202, response.text
    body = response.json()
    assert body["accepted"] is True
    assert body["duplicate"] is False
    assert body["id"] > 0

    stored = client.get("/api/v1/admin/observations", headers=admin_headers())
    assert stored.status_code == 200
    rows = stored.json()
    assert len(rows) == 1
    assert rows[0]["message_id"] == payload["message_id"]
    assert rows[0]["data"]["temperature_c"] == 21.5
    assert rows[0]["observed_at"].endswith("Z")
    assert rows[0]["received_at"].endswith("Z")


def test_duplicate_message_is_idempotent(client):
    payload = observation_payload()
    first = client.post("/api/v1/observations", json=payload, headers=device_headers())
    second = client.post("/api/v1/observations", json=payload, headers=device_headers())
    assert first.status_code == 202
    assert second.status_code == 200
    assert second.json()["duplicate"] is True
    assert second.json()["id"] == first.json()["id"]

    stored = client.get("/api/v1/admin/observations", headers=admin_headers()).json()
    assert len(stored) == 1


def test_accepts_cbor_observation(client):
    payload = observation_payload()
    response = client.post(
        "/api/v1/observations",
        content=cbor2.dumps(payload),
        headers={**device_headers(), "Content-Type": "application/cbor"},
    )
    assert response.status_code == 202, response.text


def test_rejects_missing_token(client):
    response = client.post("/api/v1/observations", json=observation_payload())
    assert response.status_code == 401


def test_shared_bearer_token_can_identify_device(client):
    response = client.post(
        "/api/v1/observations",
        json=observation_payload(),
        headers={"Authorization": "Bearer secret", "X-Device-Id": DEVICE_ID},
    )
    assert response.status_code == 202, response.text


def test_rejects_unknown_token(client):
    response = client.post(
        "/api/v1/observations",
        json=observation_payload(),
        headers=device_headers("wrong-token"),
    )
    assert response.status_code == 401


def test_rejects_device_id_mismatch(client):
    payload = observation_payload(device_id="sim-999")
    response = client.post("/api/v1/observations", json=payload, headers=device_headers())
    assert response.status_code == 403


def test_rejects_unknown_protocol_version(client):
    payload = observation_payload(v=2)
    response = client.post("/api/v1/observations", json=payload, headers=device_headers())
    assert response.status_code == 422


def test_rejects_empty_data(client):
    payload = observation_payload(data={})
    response = client.post("/api/v1/observations", json=payload, headers=device_headers())
    assert response.status_code == 422


def test_rejects_future_timestamp(client):
    future = datetime.now(UTC) + timedelta(hours=2)
    payload = observation_payload(
        observed_at=future.replace(microsecond=0).isoformat().replace("+00:00", "Z")
    )
    response = client.post("/api/v1/observations", json=payload, headers=device_headers())
    assert response.status_code == 422


def test_rejects_unsupported_content_type(client):
    response = client.post(
        "/api/v1/observations",
        content=b"temperature=21",
        headers={**device_headers(), "Content-Type": "text/plain"},
    )
    assert response.status_code == 415


def test_records_reported_firmware_version(client):
    response = client.post(
        "/api/v1/observations", json=observation_payload(), headers=device_headers()
    )
    assert response.status_code == 202
    devices = client.get("/api/v1/admin/devices", headers=admin_headers()).json()
    device = next(item for item in devices if item["id"] == DEVICE_ID)
    assert device["firmware_version"] == "0.1.0-dev"
    assert device["last_seen_at"] is not None
