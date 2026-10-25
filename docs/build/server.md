# Running the server

## Local (recommended during development)

```bash
make server-setup       # creates .venv, installs package + dev tools + cmake
make server-dev         # http://127.0.0.1:8000/docs
```

`make server-dev` runs Alembic, then Uvicorn with autoreload. The default
database is SQLite at `server/data/akashvani.db`.

To start from a clean database:

```bash
rm -f server/data/akashvani.db
cd server && ../.venv/bin/alembic upgrade head
```

## Configuration

Copy `server/.env.example` to `server/.env` when running from `server/`, or export
`AKASHVANI_*` variables. Full reference: [Configuration](../reference/configuration.md).

Minimum development variables:

```dotenv
AKASHVANI_ENVIRONMENT=development
AKASHVANI_DATABASE_URL=sqlite:///./data/akashvani.db
AKASHVANI_DEVICE_SEEDS=sim-001:dev-sim-token:Simulator
AKASHVANI_ADMIN_TOKEN=dev-admin-token
```

`AKASHVANI_DEVICE_SEEDS` is `id:token:display-name` separated by commas. Seeds
only create missing devices; a restart never rotates an existing token.

## Migrations

```bash
cd server
../.venv/bin/alembic upgrade head          # apply
../.venv/bin/alembic revision --autogenerate -m "describe change"
../.venv/bin/alembic downgrade -1          # development only
```

The Docker entrypoint applies migrations automatically. Do not let production
start without a successful migration.

## Docker

```bash
make docker-build
make docker-up
make docker-logs
make docker-down
```

The compose file:

- builds `server/` with non-root UID/GID defaults (1000:1000; override with
  `UID`/`GID`);
- maps `server/data` to `/data` for SQLite and firmware images;
- reads seeds/admin token from environment with development defaults;
- exposes port 8000.

API docs: <http://localhost:8000/docs>.

### PostgreSQL

```bash
docker compose -f server/docker-compose.yml \
  -f server/docker-compose.postgres.yml up -d --build
```

The override sets `AKASHVANI_DATABASE_URL` to the `db` service and waits for its
health check. The image installs `psycopg[binary]` for this path. Use a real
password and a managed database in production; the compose credentials are for
local use only.

## Publish an OTA release

```bash
make fw-export
```

The binary and checksum are copied to `server/data/firmware/`. Register it:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/firmware \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"version":"0.1.0-dev","channel":"stable","filename":"akashvani-receiver-0.1.0-dev.bin"}'
```

Check what a device sees:

```bash
curl -sS 'http://127.0.0.1:8000/api/v1/firmware/check?current_version=0.0.1&channel=stable' \
  -H 'X-Device-Token: dev-sim-token'
```

## Send a test observation

Use the example payload, changing `observed_at` if your environment rejects a
future/past outlier:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/observations \
  -H 'X-Device-Token: dev-sim-token' \
  -H 'Content-Type: application/json' \
  --data @protocol/examples/observation.json
```

Because the example uses `device_id: "sim-001"`, it matches the seeded device.

## Enqueue a command and observe the flow

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/devices/sim-001/commands \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"type":"sync_time"}'

curl -sS -X POST 'http://127.0.0.1:8000/api/v1/devices/me/commands/lease?limit=5' \
  -H 'X-Device-Token: dev-sim-token'
```

The real receiver polls this endpoint on its configured interval.

## Tests and lint

```bash
make server-test
make server-lint
make server-format
```

## Production notes

- Put the API behind a TLS reverse proxy; do not expose Uvicorn directly.
- Set a strong `AKASHVANI_ADMIN_TOKEN` and per-device tokens.
- Use PostgreSQL for multiple workers; validate command leasing under load.
- Persist `/data` or move firmware to object storage.
- Restrict `/api/v1/admin/*` by network policy as well as token.
- Back up the database and release artifacts.
