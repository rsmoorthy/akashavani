# Akashavani server

HTTP ingest, command and OTA service for the receiver fleet. Python 3.11+,
FastAPI, SQLAlchemy 2, Alembic. SQLite is the development default; the same
code runs on PostgreSQL through `AKASHAVANI_DATABASE_URL`.

```bash
make server-setup
make server-dev          # http://127.0.0.1:8000/docs
make server-test
make docker-up           # http://localhost:8000/docs
```

## Endpoints

| Method | Path | Auth | Purpose |
| --- | --- | --- | --- |
| GET | `/healthz` | none | liveness |
| GET | `/readyz` | none | database readiness |
| POST | `/api/v1/observations` | device token | ingest JSON or CBOR |
| GET | `/api/v1/devices/me` | device token | device identity |
| POST | `/api/v1/devices/me/commands/lease` | device token | lease pending commands |
| POST | `/api/v1/devices/me/commands/{id}/ack` | device token | acknowledge a command |
| GET | `/api/v1/firmware/check` | device token | latest OTA release |
| GET | `/api/v1/firmware/{version}/download` | device token | OTA image |
| POST | `/api/v1/admin/devices` | admin bearer | create/replace a device token |
| GET | `/api/v1/admin/devices` | admin bearer | list devices |
| POST | `/api/v1/admin/devices/{id}/commands` | admin bearer | enqueue a command |
| GET | `/api/v1/admin/observations` | admin bearer | inspect recent data |
| POST | `/api/v1/admin/firmware` | admin bearer | register an image in `firmware_dir` |
| POST | `/api/v1/admin/firmware/upload` | admin bearer | upload an image (multipart) |
| GET | `/api/v1/admin/firmware` | admin bearer | list releases |
| GET | `/api/v1/admin/devices/{id}/commands` | admin bearer | command history |
| GET | `/admin` | none (page shell) | admin UI; API calls still need the bearer token |

Device auth uses `X-Device-Token`. Admin auth uses `Authorization: Bearer
<AKASHAVANI_ADMIN_TOKEN>`. Tokens are stored as SHA-256 hashes; the plaintext is
only known to the device (or to the operator who provisioned it).

## Data model

- `devices`: identity, token hash, station ID, MAC, last seen, reported firmware.
- `observations`: canonical envelope plus source metadata. Unique
  `(device_id, message_id)` makes retries idempotent.
- `commands`: pending/delivered/acked/failed state with payload, result and
  expiry.
- `firmware_releases`: version, channel, filename, SHA-256, size, notes.

## Development versus production

- Development: SQLite, HTTP, seed device from environment.
- Production: PostgreSQL, HTTPS at the reverse proxy, strong admin token,
  per-device tokens, Alembic migrations. The application does not create tables
  automatically outside `environment=test`.

The Docker entrypoint runs `alembic upgrade head` before starting Uvicorn.
