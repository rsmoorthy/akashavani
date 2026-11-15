#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv"

if [[ ! -x "$VENV/bin/uvicorn" ]]; then
  echo "Server environment missing. Run 'make server-setup'." >&2
  exit 1
fi

cd "$ROOT/server"
"$VENV/bin/alembic" upgrade head
exec "$VENV/bin/uvicorn" akashavani_server.main:app \
  --reload \
  --host "${AKASHAVANI_HOST:-127.0.0.1}" \
  --port "${AKASHAVANI_PORT:-8000}"
