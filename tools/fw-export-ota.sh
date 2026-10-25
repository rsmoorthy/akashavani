#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/fw-common.sh"

"$ROOT/tools/fw-build.sh" receiver

BIN="$BUILD_DIR/receiver/receiver.ino.bin"
if [[ ! -f "$BIN" ]]; then
  echo "Expected $BIN after build" >&2
  exit 1
fi

OTA_DIR="$BUILD_DIR/ota"
SERVER_FW_DIR="$ROOT/server/data/firmware"
mkdir -p "$OTA_DIR" "$SERVER_FW_DIR"

ARTIFACT="akashvani-receiver-${VERSION}.bin"
cp "$BIN" "$OTA_DIR/$ARTIFACT"
cp "$BIN" "$SERVER_FW_DIR/$ARTIFACT"
(
  cd "$OTA_DIR"
  sha256sum "$ARTIFACT" > "$ARTIFACT.sha256"
)

echo
echo "OTA image: $OTA_DIR/$ARTIFACT"
echo "SHA-256:  $(cut -d' ' -f1 "$OTA_DIR/$ARTIFACT.sha256")"
echo
echo "Register with a local server:"
echo "  curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/firmware \\"
echo "    -H 'Authorization: Bearer dev-admin-token' \\"
echo "    -H 'Content-Type: application/json' \\"
echo "    -d '{\"version\":\"${VERSION}\",\"channel\":\"stable\",\"filename\":\"${ARTIFACT}\"}'"
