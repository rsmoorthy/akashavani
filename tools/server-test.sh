#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv"

if [[ ! -x "$VENV/bin/pytest" ]]; then
  echo "Server environment missing. Run 'make server-setup'." >&2
  exit 1
fi

cd "$ROOT/server"
exec "$VENV/bin/pytest" "$@"
