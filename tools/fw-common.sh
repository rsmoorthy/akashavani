#!/usr/bin/env bash
# Shared helpers for firmware scripts. Source this file; do not execute it.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
LIBRARIES_DIR="$ROOT/firmware/libraries"
BUILD_DIR="$ROOT/firmware/build"
CACHE_DIR="$ROOT/firmware/.cache/arduino"
VERSION="$(tr -d '[:space:]' < "$ROOT/firmware/VERSION")"

# Stock partition tables. A custom 16 MB table can be injected with
# AKASHVANI_FQBN once it has been validated on hardware.
FQBN_4MB="${AKASHVANI_FQBN_4MB:-esp32:esp32:esp32s3:FlashSize=4M,PSRAM=disabled,PartitionScheme=min_spiffs,CDCOnBoot=cdc}"
FQBN_16MB="${AKASHVANI_FQBN_16MB:-esp32:esp32:esp32s3:FlashSize=16M,PSRAM=disabled,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc}"

select_fqbn() {
  if [[ -n "${AKASHVANI_FQBN:-}" ]]; then
    printf '%s' "$AKASHVANI_FQBN"
  elif [[ "${AKASHVANI_FLASH_SIZE:-4M}" == "16M" ]]; then
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
    *)
      echo "Unknown board '${1:-}'. Use receiver, transmitter_sim or bringup." >&2
      return 2
      ;;
  esac
}
