#!/usr/bin/env bash
# Shared helpers for firmware scripts. Source this file; do not execute it.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
LIBRARIES_DIR="$ROOT/firmware/libraries"
BUILD_DIR="$ROOT/firmware/build"
CACHE_DIR="$ROOT/firmware/.cache/arduino"
VERSION="$(tr -d '[:space:]' < "$ROOT/firmware/VERSION")"

# The two bench boards are ESP32-D0WD-V3 (classic). The production target may
# move to ESP32-S3 later; override AKASHAVANI_FQBN for that. The receiver sketch
# carries an explicit 4 MB partition table (two 1.875 MiB OTA slots) which the
# core's pre-build hook copies into the build directory.
FQBN_4MB="${AKASHAVANI_FQBN_4MB:-esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs}"
FQBN_16MB="${AKASHAVANI_FQBN_16MB:-esp32:esp32:esp32:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB}"

select_fqbn() {
  if [[ -n "${AKASHAVANI_FQBN:-}" ]]; then
    printf '%s' "$AKASHAVANI_FQBN"
  elif [[ "${AKASHAVANI_FLASH_SIZE:-4M}" == "16M" ]]; then
    printf '%s' "$FQBN_16MB"
  else
    printf '%s' "$FQBN_4MB"
  fi
}

board_sketch() {
  case "${1:-}" in
    receiver) printf '%s' "$ROOT/firmware/sketches/receiver" ;;
    transmitter_sim|sim) printf '%s' "$ROOT/firmware/sketches/transmitter_sim" ;;
    bringup) printf '%s' "$ROOT/firmware/tests/hardware/bringup" ;;
    ap_client) printf '%s' "$ROOT/firmware/tests/hardware/ap_client" ;;
    *)
      echo "Unknown board '${1:-}'. Use receiver, transmitter_sim, bringup or ap_client." >&2
      return 2
      ;;
  esac
}
