#!/usr/bin/env bash
# Shared helpers for firmware scripts. Source this file; do not execute it.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
LIBRARIES_DIR="$ROOT/firmware/libraries"
BUILD_DIR="$ROOT/firmware/build"
CACHE_DIR="$ROOT/firmware/.cache/arduino"
VERSION="$(tr -d '[:space:]' < "$ROOT/firmware/VERSION")"

# Load firmware/.env when present. Simple KEY=VALUE lines, optional surrounding
# quotes and `#` comments; the file is parsed, not sourced, so values are not
# shell-expanded. An existing environment variable wins, which lets CI and
# one-off builds override the file.
load_env_file() {
  local file="$1"
  [[ -f "$file" ]] || return 0
  local line key value
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%$'\r'}"
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [[ "$line" == export\ * ]] && line="${line#export }"
    [[ -z "$line" || "$line" == \#* ]] && continue
    [[ "$line" == *=* ]] || continue
    key="${line%%=*}"
    value="${line#*=}"
    key="${key#"${key%%[![:space:]]*}"}"
    key="${key%"${key##*[![:space:]]}"}"
    value="${value#"${value%%[![:space:]]*}"}"
    value="${value%"${value##*[![:space:]]}"}"
    [[ "$key" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue
    if [[ ${#value} -ge 2 && "$value" == \"*\" && "$value" == *\" ]]; then
      value="${value:1:${#value}-2}"
    elif [[ ${#value} -ge 2 && "$value" == \'*\' && "$value" == *\' ]]; then
      value="${value:1:${#value}-2}"
    fi
    if [[ -z "${!key:-}" ]]; then
      export "$key=$value"
    fi
  done < "$file"
}

load_env_file "$ROOT/firmware/.env"

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
