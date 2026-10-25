#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv"
FIX="${1:-}"

if [[ ! -x "$VENV/bin/ruff" ]]; then
  echo "Server environment missing. Run 'make server-setup'." >&2
  exit 1
fi

cd "$ROOT"
if [[ "$FIX" == "--fix" ]]; then
  "$VENV/bin/ruff" format server
  "$VENV/bin/ruff" check server --fix
else
  "$VENV/bin/ruff" format --check server
  "$VENV/bin/ruff" check server
fi

cd "$ROOT/server"
"$VENV/bin/mypy" src
