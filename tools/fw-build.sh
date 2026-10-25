#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/fw-common.sh"

BOARD="${1:-receiver}"
SKETCH="$(board_sketch "$BOARD")"
OUT="$BUILD_DIR/$(basename "$SKETCH")"
FQBN="$(select_fqbn)"

if ! command -v "$ARDUINO_CLI" >/dev/null 2>&1; then
  echo "arduino-cli not found. Run 'make fw-setup' first." >&2
  exit 1
fi

mkdir -p "$OUT" "$CACHE_DIR"

echo "Building $BOARD with $FQBN"
"$ARDUINO_CLI" compile \
  --fqbn "$FQBN" \
  --libraries "$LIBRARIES_DIR" \
  --build-cache-path "$CACHE_DIR" \
  --output-dir "$OUT" \
  --warnings default \
  --build-property "build.defines=-DAKASHVANI_FW_VERSION=\"${VERSION}\"" \
  "$SKETCH"

echo
echo "Artifacts: $OUT"
