#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/fw-common.sh"

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

# arduino-cli needs an FQBN to open a port it cannot auto-identify (the bench
# boards report as "Unknown"). The FQBN does not affect the monitor settings.
FQBN="$(select_fqbn)"

exec "$ARDUINO_CLI" monitor --port "$PORT" --fqbn "$FQBN" --config "baudrate=$BAUD"
