#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-}"
BAUD="${2:-115200}"

if [[ -z "$PORT" ]]; then
  echo "Usage: $0 <serial-port> [baud]" >&2
  exit 2
fi
if [[ ! -e "$PORT" ]]; then
  echo "Serial port $PORT does not exist. Available ports:" >&2
  arduino-cli board list
  exit 1
fi

exec arduino-cli monitor --port "$PORT" --config "baudrate=$BAUD"
