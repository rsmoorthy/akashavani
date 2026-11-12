from __future__ import annotations

from tests.factories import DEVICE_ID, admin_headers, device_headers

RANGE = {"start_epoch": 1794400000, "end_epoch": 1794403600}
RECORDS = [
    {"t": 1794400100, "hex": "81 06 01 2B EE 00 6F 3B", "rssi": -40, "ch": 0},
    {"t": 1794400000, "hex": "A1 04 06 28 20 01 33 B7", "rssi": -55, "ch": 3},
]


def test_raw_batch_upload_and_csv_download(client):
    response = client.post(
        "/api/v1/devices/me/raw",
        json={**RANGE, "records": RECORDS},
        headers=device_headers(),
    )
    assert response.status_code == 202, response.text
    assert response.json()["stored"] == 2

    csv = client.get(
        f"/api/v1/admin/raw.csv?device_id={DEVICE_ID}"
        f"&start={RANGE['start_epoch']}&end={RANGE['end_epoch']}",
        headers=admin_headers(),
    )
    assert csv.status_code == 200
    lines = csv.text.strip().splitlines()
    assert lines[0] == "timestamp_ist,raw,rssi,channel"
    body = "\n".join(lines[1:])
    assert "81 06 01 2B EE 00 6F 3B" in body
    assert "A1 04 06 28 20 01 33 B7" in body
    # Sorted oldest first; both timestamps are IST.
    assert lines[1].startswith("2026-11-11T")
    assert all(line.split(",")[0].endswith("+05:30") for line in lines[1:])


def test_raw_batch_rejects_long_range(client):
    response = client.post(
        "/api/v1/devices/me/raw",
        json={"start_epoch": 0, "end_epoch": 8 * 86400, "records": RECORDS},
        headers=device_headers(),
    )
    assert response.status_code == 422


def test_raw_csv_missing_range_returns_404(client):
    response = client.get(
        f"/api/v1/admin/raw.csv?device_id={DEVICE_ID}&start=1&end=100",
        headers=admin_headers(),
    )
    assert response.status_code == 404
