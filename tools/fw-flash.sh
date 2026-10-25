#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/fw-common.sh"

BOARD="${1:-receiver}"
PORT="${2:-}"
SKETCH="$(board_sketch "$BOARD")"
OUT="$BUILD_DIR/$(basename "$SKETCH")"
FQBN="$(select_fqbn)"

if [[ -z "$PORT" ]]; then
  echo "Usage: $0 <receiver|transmitter_sim|bringup> <serial-port>" >&2
  exit 2
fi
if [[ ! -e "$PORT" ]]; then
  echo "Serial port $PORT does not exist. Available ports:" >&2
  "$ARDUINO_CLI" board list
  exit 1
fi

"$ROOT/tools/fw-build.sh" "$BOARD"

echo "Uploading $BOARD to $PORT"
"$ARDUINO_CLI" upload \
  --port "$PORT" \
  --fqbn "$FQBN" \
  --input-dir "$OUT" \
  --protocol serial \
  "$SKETCH"
