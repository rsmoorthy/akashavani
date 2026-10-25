from __future__ import annotations

import hashlib

from tests.factories import admin_headers, device_headers


def register(client, app, *, version="0.2.0", filename="firmware.bin", content=b"firmware-bytes"):
    firmware_dir = app.state.settings.firmware_dir
    firmware_dir.mkdir(parents=True, exist_ok=True)
    (firmware_dir / filename).write_bytes(content)
    return client.post(
        "/api/v1/admin/firmware",
        json={"version": version, "channel": "stable", "filename": filename, "mandatory": False},
        headers=admin_headers(),
    )


def test_register_check_and_download(client, app):
    content = b"binary-firmware-payload"
    response = register(client, app, content=content)
    assert response.status_code == 201, response.text
    release = response.json()
    assert release["version"] == "0.2.0"
    assert release["size_bytes"] == len(content)
    expected_sha = hashlib.sha256(content).hexdigest()
    assert release["sha256"] == expected_sha

    check = client.get(
        "/api/v1/firmware/check?current_version=0.1.0&channel=stable",
        headers=device_headers(),
    )
    assert check.status_code == 200, check.text
    info = check.json()
    assert info["update_available"] is True
    assert info["version"] == "0.2.0"
    assert info["sha256"] == expected_sha
    assert info["download_url"] == "/api/v1/firmware/0.2.0/download"

    download = client.get(info["download_url"], headers=device_headers())
    assert download.status_code == 200
    assert download.content == content
    assert download.headers["X-Checksum-SHA256"] == expected_sha
    assert download.headers["X-Firmware-Version"] == "0.2.0"

    up_to_date = client.get(
        "/api/v1/firmware/check?current_version=0.2.0&channel=stable",
        headers=device_headers(),
    )
    assert up_to_date.status_code == 200
    assert up_to_date.json()["update_available"] is False


def test_no_release_means_no_update(client):
    response = client.get("/api/v1/firmware/check?current_version=0.1.0", headers=device_headers())
    assert response.status_code == 200
    assert response.json()["update_available"] is False


def test_register_requires_existing_file(client, app):
    response = client.post(
        "/api/v1/admin/firmware",
        json={"version": "0.3.0", "filename": "missing.bin"},
        headers=admin_headers(),
    )
    assert response.status_code == 404


def test_duplicate_version_is_rejected(client, app):
    assert register(client, app).status_code == 201
    duplicate = register(client, app)
    assert duplicate.status_code == 409


def test_filename_must_be_plain(client, app):
    response = client.post(
        "/api/v1/admin/firmware",
        json={"version": "0.4.0", "filename": "../escape.bin"},
        headers=admin_headers(),
    )
    assert response.status_code == 422


def test_download_requires_device_token(client, app):
    assert register(client, app).status_code == 201
    response = client.get("/api/v1/firmware/0.2.0/download")
    assert response.status_code == 401
