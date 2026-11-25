from __future__ import annotations

from tests.factories import admin_headers, device_headers


def _command(client, command_type: str, payload: dict | None = None) -> dict:
    response = client.post(
        "/api/v1/admin/devices/sim-001/commands",
        json={"type": command_type, "payload": payload or {}},
        headers=admin_headers(),
    )
    assert response.status_code == 200, response.text
    return response.json()


def test_sd_list_upload_and_download(client):
    command = _command(client, "sd_list", {"path": "/archive"})
    response = client.post(
        "/api/v1/devices/me/storage/list",
        json={
            "path": "/archive",
            "truncated": False,
            "entries": [
                {"name": "sim-001", "dir": True, "size": 0, "mtime": 0},
                {"name": "2026", "dir": True, "size": 0, "mtime": 0},
            ],
        },
        headers={
            **device_headers(),
            "X-Storage-Command": command["id"],
            "X-Storage-Path": "/archive",
        },
    )
    assert response.status_code == 202, response.text

    meta = client.get(
        f"/api/v1/admin/devices/sim-001/storage/{command['id']}",
        headers=admin_headers(),
    )
    assert meta.status_code == 200, meta.text
    body = meta.json()
    assert body["kind"] == "listing"
    assert body["path"] == "/archive"
    assert body["listing"]["entries"][0]["name"] == "sim-001"
    assert body["download_url"].endswith("/download")

    download = client.get(body["download_url"], headers=admin_headers())
    assert download.status_code == 200
    assert download.headers["content-type"].startswith("application/json")
    assert b"sim-001" in download.content


def test_sd_read_upload_and_download(client):
    command = _command(client, "sd_read", {"path": "/logs/test.log"})
    payload = b'{"line": 1}\n'
    response = client.post(
        "/api/v1/devices/me/storage/file",
        content=payload,
        headers={
            **device_headers(),
            "X-Storage-Command": command["id"],
            "X-Storage-Path": "/logs/test.log",
            "X-Storage-Offset": "0",
            "X-Storage-EOF": "1",
            "Content-Type": "application/octet-stream",
        },
    )
    assert response.status_code == 202, response.text
    assert response.json()["bytes"] == len(payload)

    meta = client.get(
        f"/api/v1/admin/devices/sim-001/storage/{command['id']}",
        headers=admin_headers(),
    )
    assert meta.status_code == 200
    assert meta.json()["kind"] == "chunk"
    assert meta.json()["eof"] is True

    download = client.get(
        f"/api/v1/admin/devices/sim-001/storage/{command['id']}/download",
        headers=admin_headers(),
    )
    assert download.status_code == 200
    assert download.content == payload


def test_storage_rejects_wrong_command_type_and_bad_path(client):
    command = _command(client, "storage_status")
    response = client.post(
        "/api/v1/devices/me/storage/list",
        json={"entries": []},
        headers={
            **device_headers(),
            "X-Storage-Command": command["id"],
            "X-Storage-Path": "/archive",
        },
    )
    assert response.status_code == 422

    list_command = _command(client, "sd_list")
    response = client.post(
        "/api/v1/devices/me/storage/list",
        json={"entries": []},
        headers={
            **device_headers(),
            "X-Storage-Command": list_command["id"],
            "X-Storage-Path": "/../../etc/passwd",
        },
    )
    assert response.status_code == 422

    response = client.post(
        "/api/v1/devices/me/storage/file",
        content=b"x",
        headers={
            **device_headers(),
            "X-Storage-Command": list_command["id"],
            "X-Storage-Path": "/logs/test.log",
        },
    )
    assert response.status_code == 422
