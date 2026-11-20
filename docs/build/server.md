# Running the server

## Local (recommended during development)

```bash
make server-setup       # creates .venv, installs package + dev tools + cmake
make server-dev         # http://127.0.0.1:8000/docs
```

`make server-dev` runs Alembic, then Uvicorn with autoreload. The default
database is SQLite at `server/data/akashavani.db`.

To start from a clean database:

```bash
rm -f server/data/akashavani.db
cd server && ../.venv/bin/alembic upgrade head
```

## Configuration

Copy `server/.env.example` to `server/.env` when running from `server/`, or export
`AKASHAVANI_*` variables. Full reference: [Configuration](../reference/configuration.md).

Minimum development variables:

```dotenv
AKASHAVANI_ENVIRONMENT=development
AKASHAVANI_DATABASE_URL=sqlite:///./data/akashavani.db
AKASHAVANI_DEVICE_SEEDS=sim-001:dev-sim-token:Simulator
AKASHAVANI_ADMIN_TOKEN=dev-admin-token
```

`AKASHAVANI_DEVICE_SEEDS` is `id:token:display-name` separated by commas. Seeds
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

- builds `server/` with a non-root `app` user; the `UID`/`GID` build args
  default to 1000:1000;
- maps `server/data` to `/data` for SQLite and firmware images;
- reads seeds/admin token from environment with development defaults;
- exposes port 8000.

API docs: <http://localhost:8000/docs>.

### Data directory and UID/GID

`/data` is created in the image, but a bind mount replaces it. The container
runs as the `app` user (UID/GID 1000 by default), so the host directory must be
writable by that UID. When it is not, the container restart-loops with:

```text
sqlalchemy.exc.OperationalError: (sqlite3.OperationalError) unable to open database file
```

Nothing creates the SQLite file explicitly. `alembic upgrade head` in the
entrypoint connects first, and SQLite creates `akashavani.db` (plus its journal
file) only if the process may write to the directory. A bind source that Docker
auto-creates is owned by `root`, which produces exactly this error.

To host the data outside the repository, for example at `/mnt/akdata`, change
the volume in `server/docker-compose.yml`:

```yaml
    volumes:
      - /mnt/akdata:/data
```

Then give the directory to the container UID and set the same UID/GID in
`server/.env`:

```dotenv
# server/.env — Compose interpolation, not AKASHAVANI_* settings
UID=1001
GID=1001
```

```bash
sudo install -d -o 1001 -g 1001 -m 0755 /mnt/akdata
docker compose -f server/docker-compose.yml config | grep -A3 'args:'
# UID and GID must both read "1001" before building
make docker-build          # build args apply only at build time
make docker-up
ls -l /mnt/akdata          # akashavani.db appears after the first successful start
```

Keep the `AKASHAVANI_DATA_DIR`, `AKASHAVANI_FIRMWARE_DIR` and
`AKASHAVANI_DATABASE_URL` values under `/data`; they are container paths, not
host paths. With the mount above, host `/mnt/akdata/akashavani.db` is container
`/data/akashavani.db`, and `/mnt/akdata/firmware` is `/data/firmware`.

`UID` is a readonly bash variable and is not exported, so `${UID:-1000}` in the
compose file falls back to 1000 even when the operator's UID differs. Setting it
in `server/.env` is the reliable way to pass it. Files created by the container
are owned by the container UID, so matching the host directory owner avoids
needing `sudo` to manage uploads and exports.

### PostgreSQL

```bash
docker compose -f server/docker-compose.yml \
  -f server/docker-compose.postgres.yml up -d --build
```

The override sets `AKASHAVANI_DATABASE_URL` to the `db` service and waits for its
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
  -d '{"version":"0.1.0-dev","channel":"stable","filename":"akashavani-receiver-0.1.0-dev.bin"}'
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
- Set a strong `AKASHAVANI_ADMIN_TOKEN` and per-device tokens.
- Use PostgreSQL for multiple workers; validate command leasing under load.
- Persist `/data` or move firmware to object storage.
- Restrict `/api/v1/admin/*` by network policy as well as token.
- Back up the database and release artifacts.
