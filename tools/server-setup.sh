#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv"

if [[ ! -x "$VENV/bin/python" ]]; then
  echo "Creating Python virtual environment..."
  python3 -m venv "$VENV"
fi

"$VENV/bin/python" -m pip install --upgrade pip
"$VENV/bin/pip" install -e "$ROOT/server[dev]"
# Developer tooling, not a server runtime dependency.
"$VENV/bin/pip" install cmake ninja

echo
echo "Python environment ready: $VENV"
echo "Next: make server-dev   (or make test)"
