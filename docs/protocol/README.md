# Protocol

The wire contract is versioned and shared by firmware and server. JSON is used
during development; CBOR uses the same structure and keys.

Normative artifacts:

| File | Purpose |
| --- | --- |
| `protocol/schema/observation.schema.json` | observation envelope and canonical fields |
| `protocol/schema/command.schema.json` | command resource returned by lease |
| `protocol/schema/command-ack.schema.json` | acknowledgement body |
| `protocol/schema/firmware-check.schema.json` | OTA check response |
| `protocol/examples/*.json` | valid examples used by tests |

The Python Pydantic models in `server/src/akashvani_server/schemas.py` and the
C++ constants in `firmware/libraries/AkashvaniProtocol` mirror these schemas. A
change to one requires a change to the others and a test update.

## Versioning

- `v` is the protocol version, currently `1`.
- Additive optional fields are allowed within version 1.
- Renaming fields, changing units or changing required fields requires a new
  version and a migration plan.
- The server validates the version; unknown versions are rejected.

## Authentication

Devices send:

```http
X-Device-Token: <per-device token>
X-Firmware-Version: 0.1.0-dev
Content-Type: application/json
```

Commands and admin calls use `Authorization: Bearer <AKASHVANI_ADMIN_TOKEN>` on
the administrative endpoints. Device tokens are stored as SHA-256 hashes.

## Observation envelope

```json
{
  "v": 1,
  "type": "observation",
  "device_id": "ak-3f2a1b",
  "message_id": "ak-3f2a1b-1792908000-42",
  "observed_at": "2026-10-25T06:00:00Z",
  "sequence": 42,
  "source": {
    "protocol": "davis",
    "channel": 0,
    "transmitter_id": 1,
    "rssi_dbm": -72
  },
  "data": {
    "temperature_c": 21.5,
    "humidity_pct": 55.0,
    "pressure_hpa": 1013.2,
    "wind_speed_ms": 2.5,
    "wind_direction_deg": 180.0,
    "wind_gust_ms": 3.4,
    "rain_rate_mm_h": 0.0,
    "rain_day_mm": 1.2,
    "uv_index": 3.0,
    "solar_radiation_wm2": 350.0,
    "battery_voltage_v": 3.15
  }
}
```

### `message_id` and retries

`message_id` is the idempotency key. Generate it once per observation on the
receiver and reuse it for every retry. The current firmware format is:

```text
<device-id>-<observed-at-epoch>-<monotonic-counter>
```

The server enforces `UNIQUE(device_id, message_id)`. A duplicate returns HTTP
`200` with `"duplicate": true`; a new record returns HTTP `202`. Retrying with a
new ID creates a duplicate observation.

### Canonical data fields

| Field | Unit | Notes |
| --- | --- | --- |
| `temperature_c` | °C | |
| `humidity_pct` | % | 0–100 |
| `pressure_hpa` | hPa | station pressure or sea-level, record which |
| `wind_speed_ms` | m/s | |
| `wind_direction_deg` | degrees | 0 ≤ value < 360 |
| `wind_gust_ms` | m/s | |
| `rain_rate_mm_h` | mm/h | |
| `rain_day_mm` | mm | accumulation since local midnight |
| `uv_index` | index | |
| `solar_radiation_wm2` | W/m² | |
| `battery_voltage_v` | V | transmitter/ISS battery where available |

Unknown keys are stored by the server for forward compatibility; the firmware
only emits the fields it actually decoded.

## Commands

Admin creates a command:

```bash
curl -X POST http://localhost:8000/api/v1/admin/devices/sim-001/commands \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"type":"sync_time","payload":{},"expires_in_seconds":3600}'
```

Device lease:

```http
POST /api/v1/devices/me/commands/lease?limit=5
X-Device-Token: ...
```

Response:

```json
[
  {
    "id": "0f4d38c7-5e9e-4d2d-9f84-4c6c1e3d2b11",
    "type": "sync_time",
    "payload": {},
    "status": "delivered",
    "result": null,
    "error": null,
    "created_at": "2026-10-25T06:01:00Z",
    "delivered_at": "2026-10-25T06:01:05Z",
    "acked_at": null,
    "expires_at": "2026-10-25T07:01:00Z"
  }
]
```

Command types:

| Type | Device behaviour |
| --- | --- |
| `reboot` | acknowledge, then restart |
| `sync_time` | run SNTP, acknowledge result |
| `set_config` | not implemented yet; acknowledge error |
| `check_for_update` | acknowledge, then check/apply OTA |
| `upload_logs` | not implemented yet; acknowledge error |
| `self_test` | reserved for bring-up diagnostics |

Acknowledgement:

```http
POST /api/v1/devices/me/commands/{id}/ack
```

```json
{"status": "ok", "result": {"sntp": "synced"}}
```

A command that returns `"status": "error"` must include `error`. Acknowledging an
already acknowledged command is idempotent.

## OTA check

```http
GET /api/v1/firmware/check?current_version=0.1.0&channel=stable
X-Device-Token: ...
```

```json
{
  "update_available": true,
  "version": "0.2.0",
  "download_url": "/api/v1/firmware/0.2.0/download",
  "sha256": "9f2c...",
  "size_bytes": 1180817,
  "mandatory": false
}
```

The device prefixes its configured base URL when `download_url` starts with `/`.
The download endpoint requires the device token and returns the image with
`X-Checksum-SHA256` and `X-Firmware-Version` headers.

## CBOR

Send the same map with `Content-Type: application/cbor`. The server accepts both
encodings on `POST /api/v1/observations`. CBOR is attractive on constrained
links but adds a firmware dependency; switch only after measuring the JSON
payload size and CPU cost.

## Error handling

| Status | Meaning |
| --- | --- |
| `200` | duplicate observation / successful command operation |
| `202` | new observation accepted |
| `400` | undecodable body |
| `401` | missing/unknown device token |
| `403` | device ID mismatch or invalid admin token |
| `404` | command/release not found |
| `409` | conflicting token or duplicate firmware version |
| `413` | body over the configured limit |
| `415` | unsupported content type |
| `422` | schema or timestamp validation failure |
| `503` | database not ready |

The device retries `429`, `5xx` and transport errors with exponential backoff. It
does not retry `401/403/413/422` until configuration or code changes.
