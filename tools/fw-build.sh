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

# build.defines is additive; keep the version define and append the AP password
# when firmware/.env or the environment supplies one.
DEFINES="-DAKASHAVANI_FW_VERSION=\"${VERSION}\""
if [[ -n "${AK_AP_PASSWORD:-}" ]]; then
  if [[ "$AK_AP_PASSWORD" =~ [[:space:]] ]]; then
    echo "AK_AP_PASSWORD must not contain whitespace for a build define." >&2
    echo "Define it in firmware/sketches/receiver/secrets.h instead." >&2
    exit 1
  fi
  ESCAPED="${AK_AP_PASSWORD//\\/\\\\}"
  ESCAPED="${ESCAPED//\"/\\\"}"
  DEFINES+=" -DAK_AP_PASSWORD=\"${ESCAPED}\""
fi

echo "Building $BOARD with $FQBN"
"$ARDUINO_CLI" compile \
  --fqbn "$FQBN" \
  --libraries "$LIBRARIES_DIR" \
  --build-cache-path "$CACHE_DIR" \
  --output-dir "$OUT" \
  --warnings default \
  --build-property "build.defines=${DEFINES}" \
  "$SKETCH"

echo
echo "Artifacts: $OUT"
